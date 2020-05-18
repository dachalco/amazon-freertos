#include "FreeRTOS.h"
#include "delay-board.h"

void DelayMsMcu( uint32_t ms )
{
    const TickType_t xDelay = pdMS_TO_TICKS(ms);
    vTaskDelay(xDelay);
}
