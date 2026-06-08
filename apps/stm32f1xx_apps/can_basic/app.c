#include "app.h"
#include "stm32f1xx_hal.h"
#include "can.h"

static uint32_t s_last_tx_tick = 0U;
static uint16_t s_sensor_val = 100U;
static uint32_t s_tx_count = 0U;

void app_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Cấu hình trở kéo PULLUP cho PA11 (CAN_RX) để tránh nhiễu gây treo Mailbox trong chế độ Loopback
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Cấu hình giảm tốc độ chân PA12 (CAN_TX) xuống LOW để làm mềm sườn xung, giảm nhiễu EMI gây reset Logic Analyzer
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Cấu hình lại Baudrate và định thời CAN hợp lệ (250 kbps với APB1 = 36MHz)
    // Tổng TQ = 1 (Sync) + 5 (BS1) + 3 (BS2) = 9 TQ (Đảm bảo tối thiểu >= 4 TQ theo chuẩn CAN)
    hcan.Init.Prescaler = 16;
    hcan.Init.TimeSeg1 = CAN_BS1_5TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.Mode = CAN_MODE_LOOPBACK;
    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }

    CAN_FilterTypeDef filter;

    // Cấu hình bộ lọc CAN để cho phép gửi/nhận
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000U;
    filter.FilterIdLow = 0x0000U;
    filter.FilterMaskIdHigh = 0x0000U;
    filter.FilterMaskIdLow = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    {
        Error_Handler();
    }

    // Khởi động module CAN
    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        Error_Handler();
    }

    s_last_tx_tick = HAL_GetTick();
}

void app_process(void)
{
    uint32_t now = HAL_GetTick();

    // Chu kỳ gửi 500ms
    if ((now - s_last_tx_tick) >= 500U)
    {
        s_last_tx_tick = now;

        // Định nghĩa bản tin CAN
        CAN_TxHeaderTypeDef tx_header;
        tx_header.StdId = 0x123U; // Standard ID
        tx_header.ExtId = 0x00U;
        tx_header.IDE = CAN_ID_STD;
        tx_header.RTR = CAN_RTR_DATA;
        tx_header.DLC = 8;
        tx_header.TransmitGlobalTime = DISABLE;

        // Đóng gói dữ liệu (Payload)
        uint8_t payload[8];
        payload[0] = (uint8_t)(s_sensor_val >> 8);
        payload[1] = (uint8_t)(s_sensor_val & 0xFFU);
        
        payload[2] = (uint8_t)(s_tx_count >> 24);
        payload[3] = (uint8_t)(s_tx_count >> 16);
        payload[4] = (uint8_t)(s_tx_count >> 8);
        payload[5] = (uint8_t)(s_tx_count & 0xFFU);
        
        payload[6] = 0xABU;
        payload[7] = 0xCDU;

        uint32_t tx_mailbox;

        // Nếu có mailbox trống thì gửi đi
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0U)
        {
            if (HAL_CAN_AddTxMessage(&hcan, &tx_header, payload, &tx_mailbox) == HAL_OK)
            {
                s_tx_count++;
                
                // Nháy LED PC13 trên board STM32F103VET6 để chỉ thị trạng thái gửi
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            }
        }

        // Tăng giá trị cảm biến giả lập
        s_sensor_val += 5U;
        if (s_sensor_val > 1000U)
        {
            s_sensor_val = 100U;
        }
    }
}
