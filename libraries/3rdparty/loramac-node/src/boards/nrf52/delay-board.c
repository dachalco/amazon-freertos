#include "FreeRTOS.h"
#include "delay-board.h"

void DelayMsMcu( uint32_t ms )
{
    const TickType_t xDelay = ms / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
}
