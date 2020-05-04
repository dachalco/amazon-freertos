/*
 * FreeRTOS V202002.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Called from all the example projects to start tasks that demonstrate Amazon
 * FreeRTOS libraries.
 *
 * If the project was created using the AWS console then this file will have been
 * auto generated and only reference and start the demos that were selected in the
 * console.  If the project was obtained from a source control repository then this
 * file will reference all the available and the developer can selectively comment
 * in or out the demos to execute. */

/* The config header is always included first. */
#include "iot_config.h"

/* Includes for library initialization. */
#include "iot_demo_runner.h"
#include "platform/iot_threads.h"
#include "types/iot_network_types.h"

#include "aws_demo.h"
#include "aws_demo_config.h"
//#include "LoRaMac.h"


#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"

// From loramac-node stack
#include "board-config.h"
#include "spi.h"
#include "sx126x-board.h"

/* Forward declaration of demo entry function to be renamed from #define in aws_demo_config.h */
int DEMO_entryFUNCTION( bool awsIotMqttMode,
                        const char * pIdentifier,
                        void * pNetworkServerInfo,
                        void * pNetworkCredentialInfo,
                        const IotNetworkInterface_t * pNetworkInterface );


/* Forward declaration of network connected DEMO callback to be renamed from #define in aws_demo_config.h */
#ifdef DEMO_networkConnectedCallback
    void DEMO_networkConnectedCallback( bool awsIotMqttMode,
                                        const char * pIdentifier,
                                        void * pNetworkServerInfo,
                                        void * pNetworkCredentialInfo,
                                        const IotNetworkInterface_t * pNetworkInterface );
#else
    #define DEMO_networkConnectedCallback    ( NULL )
#endif


/* Forward declaration of network disconnected DEMO callback to be renamed from #define in aws_demo_config.h */
#ifdef DEMO_networkDisconnectedCallback
    void DEMO_networkDisconnectedCallback( const IotNetworkInterface_t * pNetworkInterface );
#else
    #define DEMO_networkDisconnectedCallback    ( NULL )
#endif

/*-----------------------------------------------------------*/
/* Interfaced pins */
#define SX1262_PIN_LED        NRF_GPIO_PIN_MAP(0, 14)  // Out: nrf52 onboard LED2. Used for status/sanity check. Does not affect chip
#define SX1262_PIN_BUSY       NRF_GPIO_PIN_MAP(1, 3)  // In :
#define SX1262_PIN_DIO_1      NRF_GPIO_PIN_MAP(1, 6)  // In : Setup to be the IRQ line. Needs GPIOTE

#define SX1262_PIN_NSS        NRF_GPIO_PIN_MAP(1, 8)  // Out: SPI Slave select
#define SX1262_PIN_MOSI       NRF_GPIO_PIN_MAP(1, 13) // Out: SPI Slave input
#define SX1262_PIN_MISO       NRF_GPIO_PIN_MAP(1, 14) // In : SPI Slave output
#define SX1262_PIN_SCK        NRF_GPIO_PIN_MAP(1, 15) // Out: SPI Slave clock

#define SX1262_PIN_SX_NRESET  NRF_GPIO_PIN_MAP(0, 3)  // Out: Active LOW shield reset

/* Probe pins for different stages of chip setup. See PCB Circuit. Read ONLY */
#define SX1262_PIN_SX_ANT_SW      NRF_GPIO_PIN_MAP(1, 10)  // In
#define SX1262_PIN_SX_XTAL_SEL    NRF_GPIO_PIN_MAP(0, 29)  // In
#define SX1262_PIN_SX_DEVICE_SEL  NRF_GPIO_PIN_MAP(0, 28)  // In
#define SX1262_PIN_SX_FREQ_SEL    NRF_GPIO_PIN_MAP(0, 4)   // In

/* The remaining pins are pass-through, already routed to VDD/GND, or don't-cares */

/* Assign appropriate IO for module, except for SPI pins which are assigned in separate init function */
void init_SX1262_Shield_IO()
{
    // Unrelate status LED
    nrf_gpio_cfg_output(SX1262_PIN_LED); 
    nrf_gpio_pin_set(SX1262_PIN_LED);

    // Interface pins, minus SPI
    nrf_gpio_cfg_input(SX1262_PIN_BUSY,  NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(SX1262_PIN_DIO_1, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SX1262_PIN_SX_NRESET); 

    // Status pins. Mostly for debugging HW
    nrf_gpio_cfg_input(SX1262_PIN_SX_ANT_SW,     NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(SX1262_PIN_SX_XTAL_SEL,   NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(SX1262_PIN_SX_DEVICE_SEL, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(SX1262_PIN_SX_FREQ_SEL,   NRF_GPIO_PIN_NOPULL);

  
    /* TODO: Interrupts will be routed through this pin. Requires GPIOTE config instead */

    // Now that all pins are configured, release chip from reset state
    nrf_gpio_pin_set(SX1262_PIN_SX_NRESET);
}

void init_SX1262_Shield_SPI()
{
    
}

void init_SX1262_Shield()
{ 
    
    //init_SX1262_Shield_IO();
    //init_SX1262_Shield_SPI();

}

void SX1262_ISR()
{
    printf("radio irq fired\n");
    while(1);
}

static TaskHandle_t mTask_lora = NULL;
void lora_test_entry()
{

    // Radio's DIO1 will route irq line through gpio, hence gpiote
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_init());



    printf("Initializing chip...");
    SpiInit(&SX126x.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX126xIoInit();
    SX126xInit(SX1262_ISR);
    printf("Done.\n");
    
    const TickType_t xSleepTick = 2000 / portTICK_PERIOD_MS;
    TickType_t count = 0;
    while(1)
    {
        nrf_gpio_pin_toggle(SX1262_PIN_LED);
        printf(".");
        if (count % 10) 
        {
            printf("\n");
        }
        vTaskDelay(xSleepTick);
    }

    vTaskDelete(NULL);
}


/**
 * @brief Runs the one demo configured in the config file.
 */
void DEMO_RUNNER_RunDemos( void )
{
    /* These demos are shared with the C SDK and perform their own initialization and cleanup. */
    /*
    static demoContext_t mqttDemoContext =
    {
        .networkTypes                = democonfigNETWORK_TYPES,
        .demoFunction                = DEMO_entryFUNCTION,
        .networkConnectedCallback    = DEMO_networkConnectedCallback,
        .networkDisconnectedCallback = DEMO_networkDisconnectedCallback
    };

    Iot_CreateDetachedThread( runDemoTask,
                              &mqttDemoContext,
                              democonfigDEMO_PRIORITY,
                              democonfigDEMO_STACKSIZE );
*/
                                  
    xTaskCreate(lora_test_entry, "lora", configMINIMAL_STACK_SIZE + 0x40, NULL, tskIDLE_PRIORITY + 1, &mTask_lora);

}




