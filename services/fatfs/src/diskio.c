/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"
#include "fatfs_sd.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD      0   /* Map Micro SD Card to physical drive 0 */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    BYTE pdrv       /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv != DEV_SD) {
        return STA_NOINIT;
    }
    return SD_disk_status(pdrv);
}

/*-----------------------------------------------------------------------*/
/* Initalize a Drive                                                     */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv       /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv != DEV_SD) {
        return STA_NOINIT;
    }
    return SD_disk_initialize(pdrv);
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    BYTE pdrv,      /* Physical drive nmuber to identify the drive */
    BYTE *buff,     /* Data buffer to store read data */
    LBA_t sector,   /* Start sector in LBA */
    UINT count      /* Number of sectors to read */
)
{
    if (pdrv != DEV_SD) {
        return RES_PARERR;
    }
    return SD_disk_read(pdrv, buff, (DWORD)sector, count);
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0
DRESULT disk_write (
    BYTE pdrv,          /* Physical drive nmuber to identify the drive */
    const BYTE *buff,   /* Data to be written */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    if (pdrv != DEV_SD) {
        return RES_PARERR;
    }
    return SD_disk_write(pdrv, buff, (DWORD)sector, count);
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
    BYTE pdrv,      /* Physical drive nmuber to identify the drive */
    BYTE cmd,       /* Control code */
    void *buff      /* Buffer to send/receive control data */
)
{
    if (pdrv != DEV_SD) {
        return RES_PARERR;
    }
    return SD_disk_ioctl(pdrv, cmd, buff);
}
