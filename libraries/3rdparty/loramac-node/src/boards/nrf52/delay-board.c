#include "FreeRTOS.h"
#include "delay-board.h"

void DelayMsMcu( uint32_t ms )
{
    TickType_t xDelay = pdMS_TO_TICKS(ms);
    
    if (xDelay == 0)
    {
        xDelay++;
    }

    vTaskDelay(xDelay);
}
