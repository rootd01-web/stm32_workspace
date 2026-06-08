#include "app.h"
#include "stm32g4xx_hal.h"
#include "fdcan.h"

static uint32_t s_last_tx_tick = 0U;
static uint16_t s_sensor_val = 100U;
static uint32_t s_tx_count = 0U;

void app_init(void)
{
    // Reconfigure FDCAN1 to Loopback mode and allocate 1 standard filter element
    hfdcan1.Init.StdFiltersNbr = 1;
    hfdcan1.Init.Mode = FDCAN_MODE_INTERNAL_LOOPBACK;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure reception filter to accept standard IDs into Rx FIFO 0
    FDCAN_FilterTypeDef filter;
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0;
    filter.FilterType = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x000U;
    filter.FilterID2 = 0x7FFU; // Accept all standard IDs

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK)
    {
        Error_Handler();
    }

    // Start the FDCAN module
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
    {
        Error_Handler();
    }

    s_last_tx_tick = HAL_GetTick();
}

void app_process(void)
{
    uint32_t now = HAL_GetTick();

    // Transmit period: 500ms
    if ((now - s_last_tx_tick) >= 500U)
    {
        s_last_tx_tick = now;

        // Configure Tx Header
        FDCAN_TxHeaderTypeDef tx_header;
        tx_header.Identifier = 0x123U; // Standard ID
        tx_header.IdType = FDCAN_STANDARD_ID;
        tx_header.TxFrameType = FDCAN_DATA_FRAME;
        tx_header.DataLength = FDCAN_DLC_BYTES_8;
        tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        tx_header.BitRateSwitch = FDCAN_BRS_OFF;
        tx_header.FDFormat = FDCAN_CLASSIC_CAN;
        tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        tx_header.MessageMarker = 0;

        // Pack Payload Data
        uint8_t payload[8];
        payload[0] = (uint8_t)(s_sensor_val >> 8);
        payload[1] = (uint8_t)(s_sensor_val & 0xFFU);
        
        payload[2] = (uint8_t)(s_tx_count >> 24);
        payload[3] = (uint8_t)(s_tx_count >> 16);
        payload[4] = (uint8_t)(s_tx_count >> 8);
        payload[5] = (uint8_t)(s_tx_count & 0xFFU);
        
        payload[6] = 0xABU;
        payload[7] = 0xCDU;

        // If Tx FIFO has free space, send the message
        if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0U)
        {
            if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, payload) == HAL_OK)
            {
                s_tx_count++;
                
                // Toggle GPIO PA5 (LED on G474RET6 board)
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            }
        }

        // Increment mock sensor value
        s_sensor_val += 5U;
        if (s_sensor_val > 1000U)
        {
            s_sensor_val = 100U;
        }
    }
}
