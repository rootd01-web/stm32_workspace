#include "fatfs_sd.h"
#include "stm32f4xx_hal.h"

extern SPI_HandleTypeDef hspi1;

/* Pin definition for SD Card CS */
#define SD_CS_PORT      GPIOA
#define SD_CS_PIN       GPIO_PIN_4

#define SD_CS_LOW()     HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

/* Card type definitions */
#define CT_NONE         0x00
#define CT_MMC          0x01
#define CT_SD1          0x02
#define CT_SD2          0x04
#define CT_SDC          (CT_SD1|CT_SD2)
#define CT_BLOCK        0x08

/* SD Commands */
#define CMD0    (0x40+0)    /* GO_IDLE_STATE */
#define CMD1    (0x40+1)    /* SEND_OP_COND */
#define CMD8    (0x40+8)    /* SEND_IF_COND */
#define CMD9    (0x40+9)    /* SEND_CSD */
#define CMD10   (0x40+10)   /* SEND_CID */
#define CMD12   (0x40+12)   /* STOP_TRANSMISSION */
#define CMD16   (0x40+16)   /* SET_BLOCKLEN */
#define CMD17   (0x40+17)   /* READ_SINGLE_BLOCK */
#define CMD18   (0x40+18)   /* READ_MULTIPLE_BLOCK */
#define CMD23   (0x40+23)   /* SET_BLOCK_COUNT */
#define CMD24   (0x40+24)   /* WRITE_BLOCK */
#define CMD25   (0x40+25)   /* WRITE_MULTIPLE_BLOCK */
#define CMD41   (0x40+41)   /* APP_SEND_OP_COND (ACMD41) */
#define CMD55   (0x40+55)   /* APP_CMD */
#define CMD58   (0x40+58)   /* READ_OCR */

static volatile DSTATUS Stat = STA_NOINIT;
static BYTE CardType = CT_NONE;

/* -------------------------------------------------------------------------
   Low-level SPI Helpers
   ------------------------------------------------------------------------- */

static void SPI_TxByte(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

static uint8_t SPI_RxByte(void)
{
    uint8_t byte_rx = 0xFF;
    uint8_t byte_tx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &byte_tx, &byte_rx, 1, HAL_MAX_DELAY);
    return byte_rx;
}

static void SD_SPI_Speed_Low(void)
{
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; /* 100MHz / 256 = ~390kHz */
    HAL_SPI_Init(&hspi1);
}

static void SD_SPI_Speed_High(void)
{
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 100MHz / 8 = 12.5MHz (safe & fast) */
    HAL_SPI_Init(&hspi1);
}

/* Wait for SD card to be ready */
static int SD_Ready(void)
{
    uint8_t res;
    uint32_t timeout = 50000;
    
    SPI_RxByte(); /* Dummy read */
    do {
        res = SPI_RxByte();
        if (res == 0xFF) {
            return 1;
        }
    } while (--timeout);
    
    return 0;
}

/* Deselect SD card and release SPI bus */
static void SD_Deselect(void)
{
    SD_CS_HIGH();
    SPI_RxByte(); /* Dummy clock to let card release MISO */
}

/* Select SD card and wait for it to be ready */
static int SD_Select(void)
{
    SD_CS_LOW();
    SPI_RxByte(); /* Dummy clock */
    if (SD_Ready()) {
        return 1;
    }
    SD_Deselect();
    return 0;
}

/* Send a command packet to the SD card */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t res, n;
    
    /* ACMD<n> is the command sequence of CMD55-CMD<n> */
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = SD_SendCmd(CMD55, 0);
        if (res > 1) return res;
    }
    
    /* Select the card */
    SD_Deselect();
    if (!SD_Select()) return 0xFF;
    
    /* Send command packet */
    SPI_TxByte(cmd);                        /* Start + Command index */
    SPI_TxByte((uint8_t)(arg >> 24));        /* Argument[31..24] */
    SPI_TxByte((uint8_t)(arg >> 16));        /* Argument[23..16] */
    SPI_TxByte((uint8_t)(arg >> 8));         /* Argument[15..8] */
    SPI_TxByte((uint8_t)arg);                /* Argument[7..0] */
    
    n = 0x01;                               /* Dummy CRC + Stop bit */
    if (cmd == CMD0) n = 0x95;              /* Valid CRC for CMD0(0) */
    if (cmd == CMD8) n = 0x87;              /* Valid CRC for CMD8(0x1AA) */
    SPI_TxByte(n);
    
    /* Receive command response */
    if (cmd == CMD12) {
        SPI_RxByte();                       /* Skip a stuff byte when CMD12 */
    }
    
    n = 10;                                 /* Wait for a valid response (R1) in timeout of 10 attempts */
    do {
        res = SPI_RxByte();
    } while ((res & 0x80) && --n);
    
    return res;
}

/* Receive a data packet from the SD card */
static int SD_RxDataBlock(uint8_t *buff, uint32_t btr)
{
    uint8_t token;
    uint32_t timeout = 20000;
    
    /* Wait for data token (0xFE) */
    do {
        token = SPI_RxByte();
    } while (token == 0xFF && --timeout);
    
    if (token != 0xFE) return 0;            /* Invalid response or timeout */
    
    /* Read data block */
    do {
        *buff++ = SPI_RxByte();
    } while (--btr);
    
    /* Discard 2 bytes CRC */
    SPI_RxByte();
    SPI_RxByte();
    
    return 1;
}

/* Send a data packet to the SD card */
static int SD_TxDataBlock(const uint8_t *buff, uint8_t token)
{
    uint8_t resp;
    uint32_t i;
    
    if (!SD_Ready()) return 0;
    
    SPI_TxByte(token);                      /* Send token */
    
    if (token != 0xFD) {                    /* If not Stop Tran token */
        /* Send 512 bytes data */
        for (i = 0; i < 512; i++) {
            SPI_TxByte(buff[i]);
        }
        /* Dummy CRC */
        SPI_TxByte(0xFF);
        SPI_TxByte(0xFF);
        
        /* Receive data response */
        resp = SPI_RxByte();
        if ((resp & 0x1F) != 0x05) {        /* If data was not accepted */
            return 0;
        }
    }
    
    return 1;
}

/* -------------------------------------------------------------------------
   Public diskio Interface Implementation
   ------------------------------------------------------------------------- */

DSTATUS SD_disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return Stat;
}

DSTATUS SD_disk_initialize(BYTE pdrv)
{
    uint8_t n, cmd, ty, ocr[4];
    uint32_t timeout;
    
    if (pdrv != 0) return STA_NOINIT;
    
    /* Reconfigure PA4 (CS) as standard GPIO Output PP with Pull-Up */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SD_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SD_CS_PORT, &GPIO_InitStruct);
    
    /* Initially hold CS High */
    SD_CS_HIGH();
    
    /* Set low SPI clock speed for initialization */
    SD_SPI_Speed_Low();
    
    /* Send 10 dummy bytes to wake up the card (CS High) */
    for (n = 10; n; n--) {
        SPI_RxByte();
    }
    
    ty = CT_NONE;
    
    /* Put card into SPI mode */
    if (SD_SendCmd(CMD0, 0) == 1) {
        /* Card is in idle state */
        if (SD_SendCmd(CMD8, 0x1AA) == 1) {
            /* SDv2 (SDHC or SDXC) card */
            for (n = 0; n < 4; n++) {
                ocr[n] = SPI_RxByte();
            }
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                /* The card can work at vdd range of 2.7-3.6V */
                timeout = 10000;
                while (SD_SendCmd(CMD55 | 0x80, 0) <= 1 && SD_SendCmd(CMD41 | 0x80, 0x40000000) && --timeout);
                if (timeout && SD_SendCmd(CMD58, 0) == 0) {
                    /* Check CCS bit in OCR */
                    for (n = 0; n < 4; n++) {
                        ocr[n] = SPI_RxByte();
                    }
                    ty = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
                }
            }
        } else {
            /* SDv1 or MMC card */
            if (SD_SendCmd(CMD55 | 0x80, 0) <= 1 && SD_SendCmd(CMD41 | 0x80, 0) <= 1) {
                ty = CT_SD1;
                cmd = CMD41 | 0x80;                 /* ACMD41 */
            } else {
                ty = CT_MMC;
                cmd = CMD1;                         /* CMD1 */
            }
            timeout = 10000;
            while (SD_SendCmd(cmd, 0) && --timeout);
            if (!timeout || SD_SendCmd(CMD16, 512) != 0) {
                /* Set block length to 512 bytes */
                ty = CT_NONE;
            }
        }
    }
    
    CardType = ty;
    SD_Deselect();
    
    if (ty) {
        Stat &= ~STA_NOINIT;            /* Clear STA_NOINIT flag */
        SD_SPI_Speed_High();             /* Switch to high speed SPI clock */
    } else {
        Stat |= STA_NOINIT;             /* Force STA_NOINIT flag */
    }
    
    return Stat;
}

DRESULT SD_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    
    /* Convert sector address to byte address if non-block-addressing card */
    if (!(CardType & CT_BLOCK)) {
        sector *= 512;
    }
    
    if (count == 1) {
        /* Read single block */
        if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512)) {
            count = 0;
        }
    } else {
        /* Read multiple blocks */
        if (SD_SendCmd(CMD18, sector) == 0) {
            do {
                if (!SD_RxDataBlock(buff, 512)) break;
                buff += 512;
            } while (--count);
            SD_SendCmd(CMD12, 0);       /* Stop transmission */
        }
    }
    SD_Deselect();
    
    return count ? RES_ERROR : RES_OK;
}

DRESULT SD_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    
    /* Convert sector address to byte address if non-block-addressing card */
    if (!(CardType & CT_BLOCK)) {
        sector *= 512;
    }
    
    if (count == 1) {
        /* Write single block */
        if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE)) {
            count = 0;
        }
    } else {
        /* Write multiple blocks */
        if (CardType & CT_SDC) {
            SD_SendCmd(CMD23 | 0x80, count);
        }
        if (SD_SendCmd(CMD25, sector) == 0) {
            do {
                if (!SD_TxDataBlock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);
            if (!SD_TxDataBlock(0, 0xFD)) { /* Stop Tran token */
                count = 1;
            }
        }
    }
    SD_Deselect();
    
    return count ? RES_ERROR : RES_OK;
}

DRESULT SD_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    DRESULT res;
    BYTE n, csd[16];
    DWORD csize;
    
    if (pdrv != 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    
    res = RES_ERROR;
    
    switch (cmd) {
        case CTRL_SYNC:
            if (SD_Select()) {
                res = RES_OK;
            }
            SD_Deselect();
            break;
            
        case GET_SECTOR_COUNT:
            /* Get number of sectors on the card */
            if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16)) {
                if ((csd[0] >> 6) == 1) {
                    /* SDC ver 2.00 (High Capacity) */
                    csize = csd[9] + ((DWORD)csd[8] << 8) + ((DWORD)(csd[7] & 0x3F) << 16) + 1;
                    *(LBA_t*)buff = csize << 10;
                } else {
                    /* SDC ver 1.XX or MMC */
                    n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                    csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
                    *(LBA_t*)buff = csize << (n - 9);
                }
                res = RES_OK;
            }
            SD_Deselect();
            break;
            
        case GET_BLOCK_SIZE:
            /* Get erase block size in sectors */
            if (CardType & CT_SD2) {
                /* SDv2: always query CSD */
                if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16)) {
                    /* Read write block write size */
                    *(DWORD*)buff = 128; /* standard 128 sectors (64KB) */
                    res = RES_OK;
                }
                SD_Deselect();
            } else {
                *(DWORD*)buff = 1;
                res = RES_OK;
            }
            break;
            
        default:
            res = RES_PARERR;
            break;
    }
    
    return res;
}
