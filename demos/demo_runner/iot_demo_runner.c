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
#include "LoRaMac.h"

#define ACTIVE_REGION LORAMAC_REGION_US915

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


void McpsConfirm()
{
    while(1);
}

void McpsIndication()
{
    while(1);
}

void MlmeConfirm()
{
    while(1);
}

void MlmeIndication()
{
    while(1);
}

void OnMacProcessNotify()
{
    while(1);
}

/*!
 * MAC status strings
 */
const char* MacStatusStrings[] =
{
    "OK",                            // LORAMAC_STATUS_OK
    "Busy",                          // LORAMAC_STATUS_BUSY
    "Service unknown",               // LORAMAC_STATUS_SERVICE_UNKNOWN
    "Parameter invalid",             // LORAMAC_STATUS_PARAMETER_INVALID
    "Frequency invalid",             // LORAMAC_STATUS_FREQUENCY_INVALID
    "Datarate invalid",              // LORAMAC_STATUS_DATARATE_INVALID
    "Frequency or datarate invalid", // LORAMAC_STATUS_FREQ_AND_DR_INVALID
    "No network joined",             // LORAMAC_STATUS_NO_NETWORK_JOINED
    "Length error",                  // LORAMAC_STATUS_LENGTH_ERROR
    "Region not supported",          // LORAMAC_STATUS_REGION_NOT_SUPPORTED
    "Skipped APP data",              // LORAMAC_STATUS_SKIPPED_APP_DATA
    "Duty-cycle restricted",         // LORAMAC_STATUS_DUTYCYCLE_RESTRICTED
    "No channel found",              // LORAMAC_STATUS_NO_CHANNEL_FOUND
    "No free channel found",         // LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND
    "Busy beacon reserved time",     // LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME
    "Busy ping-slot window time",    // LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME
    "Busy uplink collision",         // LORAMAC_STATUS_BUSY_UPLINK_COLLISION
    "Crypto error",                  // LORAMAC_STATUS_CRYPTO_ERROR
    "FCnt handler error",            // LORAMAC_STATUS_FCNT_HANDLER_ERROR
    "MAC command error",             // LORAMAC_STATUS_MAC_COMMAD_ERROR
    "ClassB error",                  // LORAMAC_STATUS_CLASS_B_ERROR
    "Confirm queue error",           // LORAMAC_STATUS_CONFIRM_QUEUE_ERROR
    "Multicast group undefined",     // LORAMAC_STATUS_MC_GROUP_UNDEFINED
    "Unknown error",                 // LORAMAC_STATUS_ERROR
};

/*!
 * Executes the network Join request
 */
#define LORAWAN_DEFAULT_DATARATE                    DR_0
static bool JoinNetwork( void )
{
    LoRaMacStatus_t status;
    MlmeReq_t mlmeReq;
    mlmeReq.Type = MLME_JOIN;
    mlmeReq.Req.Join.Datarate = LORAWAN_DEFAULT_DATARATE;

    // Starts the join procedure
    status = LoRaMacMlmeRequest( &mlmeReq );
    printf( "\n###### ===== MLME-Request - MLME_JOIN ==== ######\n" );
    printf( "STATUS      : %s\n", MacStatusStrings[status] );

    if( status == LORAMAC_STATUS_OK )
    {
        printf( "###### ===== JOINING ==== ######\n" );
        return true;
    }
    else
    {
        if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
        {
            printf( "Next Tx in  : ~%lu second(s)\n", ( mlmeReq.ReqReturn.DutyCycleWaitTime / 1000 ) );
        }
        return false;
        //configASSERT(0);
    }
}


static TaskHandle_t mTask_lora = NULL;
void lora_test_entry()
{

    // Radio's DIO1 will route irq line through gpio, hence gpiote
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_init());

    printf("Initializing...");
    SpiInit(&SX126x.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX126xIoInit();

    LoRaMacPrimitives_t macPrimitives;
    LoRaMacCallback_t macCallbacks;
    macPrimitives.MacMcpsConfirm = McpsConfirm;
    macPrimitives.MacMcpsIndication = McpsIndication;
    macPrimitives.MacMlmeConfirm = MlmeConfirm;
    macPrimitives.MacMlmeIndication = MlmeIndication;
    macCallbacks.GetBatteryLevel = NULL;
    macCallbacks.GetTemperatureLevel = NULL;
    macCallbacks.NvmContextChange = NULL;
    macCallbacks.MacProcessNotify = OnMacProcessNotify;
    LoRaMacStatus_t status = LoRaMacInitialization( &macPrimitives, &macCallbacks, ACTIVE_REGION );
    if ( status != LORAMAC_STATUS_OK )
    {
        printf( "LoRaMac wasn't properly initialized, error: %s", MacStatusStrings[status] );
        // Fatal error, endless loop.
        configASSERT(0);
    } else {
           printf("SUCCESS.\n");
    }

    //
    JoinNetwork();

    // Start the routine. Report on status
    const TickType_t xSleepTick = 2000 / portTICK_PERIOD_MS;
    TickType_t count = 0;
    uint8_t  chip_cmd_status = 0;
    RadioError_t chip_errors = { 0 };
    while(1)
    {
        // Report chip status and errors
        printf("Status: %2x\n", SX126xReadCommand( RADIO_GET_STATUS, NULL, 0 ));
        chip_errors = SX126xGetDeviceErrors();
        printf("Errors: %4x\n", chip_errors.Value);
        JoinNetwork();
        
        // Status aesthetics
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




