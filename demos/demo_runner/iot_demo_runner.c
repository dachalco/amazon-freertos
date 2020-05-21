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
#include "Commissioning.h"
#include "lora-debug.h"


#define ACTIVE_REGION LORAMAC_REGION_US915

#define OVER_THE_AIR_ACTIVATION 1


#define APP_KEY { 0xBD, 0x6D, 0x98, 0x17, 0x99, 0x1C, 0xA2, 0x6F, 0xE3, 0xE9, 0x7A, 0x4D, 0x91, 0x3A, 0x82, 0xF2 } 


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



#ifndef ACTIVE_REGION

#warning "No active region defined, LORAMAC_REGION_EU868 will be used as default."

#define ACTIVE_REGION LORAMAC_REGION_EU868

#endif

/*!
 * Defines the application data transmission duty cycle. 5s, value in [ms].
 */
#define APP_TX_DUTYCYCLE                            5000

/*!
 * Defines a random delay for application data transmission duty cycle. 1s,
 * value in [ms].
 */
#define APP_TX_DUTYCYCLE_RND                        1000

/*!
 * Default datarate
 */
#define LORAWAN_DEFAULT_DATARATE                    DR_0

/*!
 * LoRaWAN confirmed messages
 */
#define LORAWAN_CONFIRMED_MSG_ON                    false

/*!
 * LoRaWAN Adaptive Data Rate
 *
 * \remark Please note that when ADR is enabled the end-device should be static
 */
#define LORAWAN_ADR_ON                              1

#if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )

#include "LoRaMacTest.h"

/*!
 * LoRaWAN ETSI duty cycle control enable/disable
 *
 * \remark Please note that ETSI mandates duty cycled transmissions. Use only for test purposes
 */
#define LORAWAN_DUTYCYCLE_ON                        true

#endif

/*!
 * LoRaWAN application port
 */
#define LORAWAN_APP_PORT                            2


#if( OVER_THE_AIR_ACTIVATION == 0 )
/*!
 * Device address
 */
static uint32_t DevAddr = LORAWAN_DEVICE_ADDRESS;
#endif

/*!
 * Application port
 */
static uint8_t AppPort = LORAWAN_APP_PORT;

/*!
 * User application data size
 */
static uint8_t AppDataSize = 1;
static uint8_t AppDataSizeBackup = 1;

/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_MAX_SIZE                           242

/*!
 * User application data
 */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_MAX_SIZE];

/*!
 * Indicates if the node is sending confirmed or unconfirmed messages
 */
static uint8_t IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;

/*!
 * Defines the application data transmission duty cycle
 */
static uint32_t TxDutyCycleTime;

/*!
 * Timer to handle the application data transmission duty cycle
 */
//static TimerEvent_t TxNextPacketTimer;
static TimerHandle_t TxNextPacketTimer;

/*!
 * Specifies the state of the application LED
 */
static bool AppLedStateOn = false;

/*!
 * Timer to handle the state of LED1
 */
static TimerEvent_t Led1Timer;

/*!
 * Timer to handle the state of LED2
 */
static TimerEvent_t Led2Timer;

/*!
 * Indicates if a new packet can be sent
 */
static bool NextTx = true;

/*!
 * Indicates if LoRaMacProcess call is pending.
 * 
 * \warning If variable is equal to 0 then the MCU can be set in low power mode
 */
static uint8_t IsMacProcessPending = 0;

/*!
 * Device states
 */
static enum eDeviceState
{
    DEVICE_STATE_RESTORE,
    DEVICE_STATE_START,
    DEVICE_STATE_JOIN,
    DEVICE_STATE_SEND,
    DEVICE_STATE_CYCLE,
    DEVICE_STATE_SLEEP
}DeviceState;

/*!
 * LoRaWAN compliance tests support data
 */
struct ComplianceTest_s
{
    bool Running;
    uint8_t State;
    bool IsTxConfirmed;
    uint8_t AppPort;
    uint8_t AppDataSize;
    uint8_t *AppDataBuffer;
    uint16_t DownLinkCounter;
    bool LinkCheck;
    uint8_t DemodMargin;
    uint8_t NbGateways;
}ComplianceTest;

/*!
 *
 */
typedef enum
{
    LORAMAC_HANDLER_UNCONFIRMED_MSG = 0,
    LORAMAC_HANDLER_CONFIRMED_MSG = !LORAMAC_HANDLER_UNCONFIRMED_MSG
}LoRaMacHandlerMsgTypes_t;

/*!
 * Application data structure
 */
typedef struct LoRaMacHandlerAppData_s
{
    LoRaMacHandlerMsgTypes_t MsgType;
    uint8_t Port;
    uint8_t BufferSize;
    uint8_t *Buffer;
}LoRaMacHandlerAppData_t;

LoRaMacHandlerAppData_t AppData =
{
    .MsgType = LORAMAC_HANDLER_UNCONFIRMED_MSG,
    .Buffer = NULL,
    .BufferSize = 0,
    .Port = 0
};

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
 * MAC event info status strings.
 */
const char* EventInfoStatusStrings[] =
{ 
    "OK",                            // LORAMAC_EVENT_INFO_STATUS_OK
    "Error",                         // LORAMAC_EVENT_INFO_STATUS_ERROR
    "Tx timeout",                    // LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT
    "Rx 1 timeout",                  // LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT
    "Rx 2 timeout",                  // LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT
    "Rx1 error",                     // LORAMAC_EVENT_INFO_STATUS_RX1_ERROR
    "Rx2 error",                     // LORAMAC_EVENT_INFO_STATUS_RX2_ERROR
    "Join failed",                   // LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL
    "Downlink repeated",             // LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED
    "Tx DR payload size error",      // LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR
    "Downlink too many frames loss", // LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS
    "Address fail",                  // LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL
    "MIC fail",                      // LORAMAC_EVENT_INFO_STATUS_MIC_FAIL
    "Multicast fail",                // LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL
    "Beacon locked",                 // LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED
    "Beacon lost",                   // LORAMAC_EVENT_INFO_STATUS_BEACON_LOST
    "Beacon not found"               // LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND
};

/*!
 * Prints the provided buffer in HEX
 * 
 * \param buffer Buffer to be printed
 * \param size   Buffer size to be printed
 */
void PrintHexBuffer( uint8_t *buffer, uint8_t size )
{
    uint8_t newline = 0;

    for( uint8_t i = 0; i < size; i++ )
    {
        if( newline != 0 )
        {
            printf( "\n" );
            newline = 0;
        }

        printf( "%02X ", buffer[i] );

        if( ( ( i + 1 ) % 16 ) == 0 )
        {
            newline = 1;
        }
    }
    printf( "\n" );
}

/*!
 * Executes the network Join request
 */
static void JoinNetwork( void )
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
        DeviceState = DEVICE_STATE_SLEEP;
    }
    else
    {
        if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
        {
            printf( "Next Tx in  : ~%lu second(s)\n", ( mlmeReq.ReqReturn.DutyCycleWaitTime / 1000 ) );
        }
        DeviceState = DEVICE_STATE_CYCLE;
    }
}

/*!
 * \brief   Prepares the payload of the frame
 */
static void PrepareTxFrame( uint8_t port )
{
    switch( port )
    {
    case 2:
        {
            AppDataSizeBackup = AppDataSize = 1;
            AppDataBuffer[0] = AppLedStateOn;
        }
        break;
    case 224:
        if( ComplianceTest.LinkCheck == true )
        {
            ComplianceTest.LinkCheck = false;
            AppDataSize = 3;
            AppDataBuffer[0] = 5;
            AppDataBuffer[1] = ComplianceTest.DemodMargin;
            AppDataBuffer[2] = ComplianceTest.NbGateways;
            ComplianceTest.State = 1;
        }
        else
        {
            switch( ComplianceTest.State )
            {
            case 4:
                ComplianceTest.State = 1;
                break;
            case 1:
                AppDataSize = 2;
                AppDataBuffer[0] = ComplianceTest.DownLinkCounter >> 8;
                AppDataBuffer[1] = ComplianceTest.DownLinkCounter;
                break;
            }
        }
        break;
    default:
        break;
    }
}

/*!
 * \brief   Prepares the payload of the frame
 *
 * \retval  [0: frame could be send, 1: error]
 */
static bool SendFrame( void )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;

    if( LoRaMacQueryTxPossible( AppDataSize, &txInfo ) != LORAMAC_STATUS_OK )
    {
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
    }
    else
    {
        if( IsTxConfirmed == false )
        {
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = AppPort;
            mcpsReq.Req.Unconfirmed.fBuffer = AppDataBuffer;
            mcpsReq.Req.Unconfirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = AppPort;
            mcpsReq.Req.Confirmed.fBuffer = AppDataBuffer;
            mcpsReq.Req.Confirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
    }

    // Update global variable
    AppData.MsgType = ( mcpsReq.Type == MCPS_CONFIRMED ) ? LORAMAC_HANDLER_CONFIRMED_MSG : LORAMAC_HANDLER_UNCONFIRMED_MSG;
    AppData.Port = mcpsReq.Req.Unconfirmed.fPort;
    AppData.Buffer = mcpsReq.Req.Unconfirmed.fBuffer;
    AppData.BufferSize = mcpsReq.Req.Unconfirmed.fBufferSize;

    LoRaMacStatus_t status;
    status = LoRaMacMcpsRequest( &mcpsReq );
    printf( "\n###### ===== MCPS-Request ==== ######\n" );
    printf( "STATUS      : %s\n", MacStatusStrings[status] );

    if( status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED )
    {
        printf( "Next Tx in  : ~%lu second(s)\n", ( mcpsReq.ReqReturn.DutyCycleWaitTime / 1000 ) );
    }

    if( status == LORAMAC_STATUS_OK )
    {
        return false;
    }
    return true;
}

/*!
 * \brief Function executed on TxNextPacket Timeout event
 */
static void OnTxNextPacketTimerEvent( void* context )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    //TimerStop( &TxNextPacketTimer );
    FreeRTOS_TimerStop( TxNextPacketTimer );

    mibReq.Type = MIB_NETWORK_ACTIVATION;
    status = LoRaMacMibGetRequestConfirm( &mibReq );

    if( status == LORAMAC_STATUS_OK )
    {
        if( mibReq.Param.NetworkActivation == ACTIVATION_TYPE_NONE )
        {
            // Network not joined yet. Try to join again
            JoinNetwork( );
        }
        else
        {
            DeviceState = DEVICE_STATE_SEND;
            NextTx = true;
        }
    }
}




/*!
 * \brief   MCPS-Confirm event function
 *
 * \param   [IN] mcpsConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void McpsConfirm( McpsConfirm_t *mcpsConfirm )
{
    printf( "\n###### ===== MCPS-Confirm ==== ######\n" );
    printf( "STATUS      : %s\n", EventInfoStatusStrings[mcpsConfirm->Status] );
    if( mcpsConfirm->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
    }
    else
    {
        switch( mcpsConfirm->McpsRequest )
        {
            case MCPS_UNCONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                break;
            }
            case MCPS_CONFIRMED:
            {
                // Check Datarate
                // Check TxPower
                // Check AckReceived
                // Check NbTrials
                break;
            }
            case MCPS_PROPRIETARY:
            {
                break;
            }
            default:
                break;
        }

        // Switch LED 1 ON
        //GpioWrite( &Led1, 1 );
        //TimerStart( &Led1Timer );
    }
    MibRequestConfirm_t mibGet;
    MibRequestConfirm_t mibReq;

    mibReq.Type = MIB_DEVICE_CLASS;
    LoRaMacMibGetRequestConfirm( &mibReq );

    printf( "\n###### ===== UPLINK FRAME %lu ==== ######\n", mcpsConfirm->UpLinkCounter );
    printf( "\n" );

    printf( "CLASS       : %c\n", "ABC"[mibReq.Param.Class] );
    printf( "\n" );
    printf( "TX PORT     : %d\n", AppData.Port );

    if( AppData.BufferSize != 0 )
    {
        printf( "TX DATA     : " );
        if( AppData.MsgType == LORAMAC_HANDLER_CONFIRMED_MSG )
        {
            printf( "CONFIRMED - %s\n", ( mcpsConfirm->AckReceived != 0 ) ? "ACK" : "NACK" );
        }
        else
        {
            printf( "UNCONFIRMED\n" );
        }
        PrintHexBuffer( AppData.Buffer, AppData.BufferSize );
    }

    printf( "\n" );
    printf( "DATA RATE   : DR_%d\n", mcpsConfirm->Datarate );

    mibGet.Type  = MIB_CHANNELS;
    if( LoRaMacMibGetRequestConfirm( &mibGet ) == LORAMAC_STATUS_OK )
    {
        printf( "U/L FREQ    : %lu\n", mibGet.Param.ChannelList[mcpsConfirm->Channel].Frequency );
    }

    printf( "TX POWER    : %d\n", mcpsConfirm->TxPower );

    mibGet.Type  = MIB_CHANNELS_MASK;
    if( LoRaMacMibGetRequestConfirm( &mibGet ) == LORAMAC_STATUS_OK )
    {
        printf("CHANNEL MASK: ");
#if defined( REGION_AS923 ) || defined( REGION_CN779 ) || \
    defined( REGION_EU868 ) || defined( REGION_IN865 ) || \
    defined( REGION_KR920 ) || defined( REGION_EU433 ) || \
    defined( REGION_RU864 )

        for( uint8_t i = 0; i < 1; i++)

#elif defined( REGION_AU915 ) || defined( REGION_US915 ) || defined( REGION_CN470 )

        for( uint8_t i = 0; i < 5; i++)
#else

#error "Please define a region in the compiler options."

#endif
        {
            printf("%04X ", mibGet.Param.ChannelsMask[i] );
        }
        printf("\n");
    }

    printf( "\n" );
}

/*!
 * \brief   MCPS-Indication event function
 *
 * \param   [IN] mcpsIndication - Pointer to the indication structure,
 *               containing indication attributes.
 */
static void McpsIndication( McpsIndication_t *mcpsIndication )
{
    printf( "\n###### ===== MCPS-Indication ==== ######\n" );
    printf( "STATUS      : %s\n", EventInfoStatusStrings[mcpsIndication->Status] );
    if( mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
        return;
    }

    switch( mcpsIndication->McpsIndication )
    {
        case MCPS_UNCONFIRMED:
        {
            break;
        }
        case MCPS_CONFIRMED:
        {
            break;
        }
        case MCPS_PROPRIETARY:
        {
            break;
        }
        case MCPS_MULTICAST:
        {
            break;
        }
        default:
            break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    if( mcpsIndication->FramePending == true )
    {
        // The server signals that it has pending data to be sent.
        // We schedule an uplink as soon as possible to flush the server.
        OnTxNextPacketTimerEvent( NULL );
    }
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot

    if( ComplianceTest.Running == true )
    {
        ComplianceTest.DownLinkCounter++;
    }

    if( mcpsIndication->RxData == true )
    {
        switch( mcpsIndication->Port )
        {
        case 1: // The application LED can be controlled on port 1 or 2
        case 2:
            if( mcpsIndication->BufferSize == 1 )
            {
                AppLedStateOn = mcpsIndication->Buffer[0] & 0x01;
            }
            break;
        case 224:
            if( ComplianceTest.Running == false )
            {
                // Check compliance test enable command (i)
                if( ( mcpsIndication->BufferSize == 4 ) &&
                    ( mcpsIndication->Buffer[0] == 0x01 ) &&
                    ( mcpsIndication->Buffer[1] == 0x01 ) &&
                    ( mcpsIndication->Buffer[2] == 0x01 ) &&
                    ( mcpsIndication->Buffer[3] == 0x01 ) )
                {
                    IsTxConfirmed = false;
                    AppPort = 224;
                    AppDataSizeBackup = AppDataSize;
                    AppDataSize = 2;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.LinkCheck = false;
                    ComplianceTest.DemodMargin = 0;
                    ComplianceTest.NbGateways = 0;
                    ComplianceTest.Running = true;
                    ComplianceTest.State = 1;

                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = true;
                    LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )
                    LoRaMacTestSetDutyCycleOn( false );
#endif
                }
            }
            else
            {
                ComplianceTest.State = mcpsIndication->Buffer[0];
                switch( ComplianceTest.State )
                {
                case 0: // Check compliance test disable command (ii)
                    IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;
                    AppPort = LORAWAN_APP_PORT;
                    AppDataSize = AppDataSizeBackup;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.Running = false;

                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                    LoRaMacMibSetRequestConfirm( &mibReq );
#if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )
                    LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
#endif
                    break;
                case 1: // (iii, iv)
                    AppDataSize = 2;
                    break;
                case 2: // Enable confirmed messages (v)
                    IsTxConfirmed = true;
                    ComplianceTest.State = 1;
                    break;
                case 3:  // Disable confirmed messages (vi)
                    IsTxConfirmed = false;
                    ComplianceTest.State = 1;
                    break;
                case 4: // (vii)
                    AppDataSize = mcpsIndication->BufferSize;

                    AppDataBuffer[0] = 4;
                    for( uint8_t i = 1; i < MIN( AppDataSize, LORAWAN_APP_DATA_MAX_SIZE ); i++ )
                    {
                        AppDataBuffer[i] = mcpsIndication->Buffer[i] + 1;
                    }
                    break;
                case 5: // (viii)
                    {
                        MlmeReq_t mlmeReq;
                        mlmeReq.Type = MLME_LINK_CHECK;
                        LoRaMacStatus_t status = LoRaMacMlmeRequest( &mlmeReq );
                        printf( "\n###### ===== MLME-Request - MLME_LINK_CHECK ==== ######\n" );
                        printf( "STATUS      : %s\n", MacStatusStrings[status] );
                    }
                    break;
                case 6: // (ix)
                    {
                        // Disable TestMode and revert back to normal operation
                        IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;
                        AppPort = LORAWAN_APP_PORT;
                        AppDataSize = AppDataSizeBackup;
                        ComplianceTest.DownLinkCounter = 0;
                        ComplianceTest.Running = false;

                        MibRequestConfirm_t mibReq;
                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                        LoRaMacMibSetRequestConfirm( &mibReq );
#if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )
                        LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
#endif

                        JoinNetwork( );
                    }
                    break;
                case 7: // (x)
                    {
                        if( mcpsIndication->BufferSize == 3 )
                        {
                            MlmeReq_t mlmeReq;
                            mlmeReq.Type = MLME_TXCW;
                            mlmeReq.Req.TxCw.Timeout = ( uint16_t )( ( mcpsIndication->Buffer[1] << 8 ) | mcpsIndication->Buffer[2] );
                            LoRaMacStatus_t status = LoRaMacMlmeRequest( &mlmeReq );
                            printf( "\n###### ===== MLME-Request - MLME_TXCW ==== ######\n" );
                            printf( "STATUS      : %s\n", MacStatusStrings[status] );
                        }
                        else if( mcpsIndication->BufferSize == 7 )
                        {
                            MlmeReq_t mlmeReq;
                            mlmeReq.Type = MLME_TXCW_1;
                            mlmeReq.Req.TxCw.Timeout = ( uint16_t )( ( mcpsIndication->Buffer[1] << 8 ) | mcpsIndication->Buffer[2] );
                            mlmeReq.Req.TxCw.Frequency = ( uint32_t )( ( mcpsIndication->Buffer[3] << 16 ) | ( mcpsIndication->Buffer[4] << 8 ) | mcpsIndication->Buffer[5] ) * 100;
                            mlmeReq.Req.TxCw.Power = mcpsIndication->Buffer[6];
                            LoRaMacStatus_t status = LoRaMacMlmeRequest( &mlmeReq );
                            printf( "\n###### ===== MLME-Request - MLME_TXCW1 ==== ######\n" );
                            printf( "STATUS      : %s\n", MacStatusStrings[status] );
                        }
                        ComplianceTest.State = 1;
                    }
                    break;
                case 8: // Send DeviceTimeReq
                    {
                        MlmeReq_t mlmeReq;

                        mlmeReq.Type = MLME_DEVICE_TIME;

                        LoRaMacStatus_t status = LoRaMacMlmeRequest( &mlmeReq );
                        printf( "\n###### ===== MLME-Request - MLME_DEVICE_TIME ==== ######\n" );
                        printf( "STATUS      : %s\n", MacStatusStrings[status] );
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    // Switch LED 2 ON for each received downlink
    //GpioWrite( &Led2, 1 );
    //TimerStart( &Led2Timer );

    const char *slotStrings[] = { "1", "2", "C", "C Multicast", "B Ping-Slot", "B Multicast Ping-Slot" };

    printf( "\n###### ===== DOWNLINK FRAME %lu ==== ######\n", mcpsIndication->DownLinkCounter );

    printf( "RX WINDOW   : %s\n", slotStrings[mcpsIndication->RxSlot] );
    
    printf( "RX PORT     : %d\n", mcpsIndication->Port );

    if( mcpsIndication->BufferSize != 0 )
    {
        printf( "RX DATA     : \n" );
        PrintHexBuffer( mcpsIndication->Buffer, mcpsIndication->BufferSize );
    }

    printf( "\n" );
    printf( "DATA RATE   : DR_%d\n", mcpsIndication->RxDatarate );
    printf( "RX RSSI     : %d\n", mcpsIndication->Rssi );
    printf( "RX SNR      : %d\n", mcpsIndication->Snr );

    printf( "\n" );
}

/*!
 * \brief   MLME-Confirm event function
 *
 * \param   [IN] mlmeConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void MlmeConfirm( MlmeConfirm_t *mlmeConfirm )
{
    printf( "\n###### ===== MLME-Confirm ==== ######\n" );
    printf( "STATUS      : %s\n", EventInfoStatusStrings[mlmeConfirm->Status] );
    if( mlmeConfirm->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
    }
    switch( mlmeConfirm->MlmeRequest )
    {
        case MLME_JOIN:
        {
            if( mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
            {
                MibRequestConfirm_t mibGet;
                printf( "###### ===== JOINED ==== ######\n" );
                printf( "\nOTAA\n\n" );

                mibGet.Type = MIB_DEV_ADDR;
                LoRaMacMibGetRequestConfirm( &mibGet );
                printf( "DevAddr     : %08lX\n", mibGet.Param.DevAddr );

                printf( "\n\n" );
                mibGet.Type = MIB_CHANNELS_DATARATE;
                LoRaMacMibGetRequestConfirm( &mibGet );
                printf( "DATA RATE   : DR_%d\n", mibGet.Param.ChannelsDatarate );
                printf( "\n" );
                // Status is OK, node has joined the network
                DeviceState = DEVICE_STATE_SEND;
            }
            else
            {
                // Join was not successful. Try to join again
                JoinNetwork( );
            }
            break;
        }
        case MLME_LINK_CHECK:
        {
            if( mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
            {
                // Check DemodMargin
                // Check NbGateways
                if( ComplianceTest.Running == true )
                {
                    ComplianceTest.LinkCheck = true;
                    ComplianceTest.DemodMargin = mlmeConfirm->DemodMargin;
                    ComplianceTest.NbGateways = mlmeConfirm->NbGateways;
                }
            }
            break;
        }
        default:
            break;
    }
}

/*!
 * \brief   MLME-Indication event function
 *
 * \param   [IN] mlmeIndication - Pointer to the indication structure.
 */
static void MlmeIndication( MlmeIndication_t *mlmeIndication )
{
    if( mlmeIndication->Status != LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED )
    {
        printf( "\n###### ===== MLME-Indication ==== ######\n" );
        printf( "STATUS      : %s\n", EventInfoStatusStrings[mlmeIndication->Status] );
    }
    if( mlmeIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
    }
    switch( mlmeIndication->MlmeIndication )
    {
        case MLME_SCHEDULE_UPLINK:
        {// The MAC signals that we shall provide an uplink as soon as possible
            OnTxNextPacketTimerEvent( NULL );
            break;
        }
        default:
            break;
    }
}

void OnMacProcessNotify( void )
{
    IsMacProcessPending = 1;
}




static TaskHandle_t mTask_lora = NULL;
void lora_test_entry()
{

    // Radio's DIO1 will route irq line through gpio, hence gpiote
    configASSERT(NRF_SUCCESS == nrf_drv_gpiote_init());

    // LEDs used to help debug RX/TX windows
    nrf_gpio_cfg_output(LED_APP_TOGGLE); 
    nrf_gpio_cfg_output(LED_TX_TOGGLE); 
    nrf_gpio_cfg_output(LED_RX_TOGGLE);
    nrf_gpio_pin_set(LED_APP_TOGGLE);
    nrf_gpio_pin_set(LED_TX_TOGGLE);
    nrf_gpio_pin_set(LED_RX_TOGGLE);
    
    StartDebugTimer();
    //printf("[DEBUG] App Start @ RTC:0d%d\n", GetDebugTime());
    GetDebugTime(APP_START);

    // Here on is to mirror their classA application code
    printf("Initializing...");
    SpiInit(&SX126x.Spi, SPI_1, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
    SX126xIoInit();
    RtcInit();

    LoRaMacPrimitives_t macPrimitives;
    LoRaMacCallback_t macCallbacks;
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    uint8_t devEui[8] = { 0 };
    uint8_t joinEui[8] = { 0 };
    uint8_t sePin[4] = { 0 };                                                                          

    macPrimitives.MacMcpsConfirm = McpsConfirm;
    macPrimitives.MacMcpsIndication = McpsIndication;
    macPrimitives.MacMlmeConfirm = MlmeConfirm;
    macPrimitives.MacMlmeIndication = MlmeIndication;
    macCallbacks.GetBatteryLevel = NULL;
    macCallbacks.GetTemperatureLevel = NULL;
    macCallbacks.NvmContextChange = NULL;
    macCallbacks.MacProcessNotify = OnMacProcessNotify;
    status = LoRaMacInitialization( &macPrimitives, &macCallbacks, ACTIVE_REGION );
    if ( status != LORAMAC_STATUS_OK )
    {
        printf( "LoRaMac wasn't properly initialized, error: %s", MacStatusStrings[status] );
        // Fatal error, endless loop.
        configASSERT(0);
    } else {
           printf("SUCCESS.\n");
    }


    // Set Dev EUI
    mibReq.Type = MIB_DEV_EUI;
    //uint8_t dev_eui[] = { 0x00, 0x31, 0x7B, 0x1E, 0xEA, 0xD4, 0x76, 0x05 };
    uint8_t dev_eui[] = { 0x00, 0x99, 0x4B, 0x20, 0x69, 0x73, 0x06, 0xD1 };
    mibReq.Param.DevEui = dev_eui;
    LoRaMacMibSetRequestConfirm( &mibReq );

    // Set App EUI
    mibReq.Type = MIB_JOIN_EUI;
    //uint8_t app_eui[] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0xD1, 0xDF };
    uint8_t app_eui[] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0xF1, 0xAD };
    mibReq.Param.JoinEui = app_eui;
    LoRaMacMibSetRequestConfirm( &mibReq );

    // Set the APP_KEY
    //mibReq.Type = MIB_APP_KEY;
    //uint8_t app_key[] = { 0xBD, 0x6D, 0x98, 0x17, 0x99, 0x1C, 0xA2, 0x6F, 0xE3, 0xE9, 0x7A, 0x4D, 0x91, 0x3A, 0x82, 0xF2 };
    //uint8_t app_key[] = { 0x55, 0x0C, 0x24, 0x1E, 0x52, 0x87, 0xF4, 0xEC, 0xF8, 0x93, 0xC5, 0x85, 0x8B, 0x7B, 0xE6, 0x72 };
    //mibReq.Param.AppKey = app_key;
    LoRaMacMibSetRequestConfirm( &mibReq );

    // Set NWK_KEY. For some reason this stack is using it to calculate MIC for join-request. Strictly for join request, MIC should use App-Key
    mibReq.Type = MIB_NWK_KEY;
    uint8_t app_key[] = { 0x55, 0x0C, 0x24, 0x1E, 0x52, 0x87, 0xF4, 0xEC, 0xF8, 0x93, 0xC5, 0x85, 0x8B, 0x7B, 0xE6, 0x72 };
    mibReq.Param.AppKey = app_key;
    LoRaMacMibSetRequestConfirm( &mibReq ); // Reuse app key


    // Start the routine. Report on status
    const TickType_t xSleepTick = 2000 / portTICK_PERIOD_MS;
    TickType_t count = 0;
    uint8_t  chip_cmd_status = 0;
    RadioError_t chip_errors = { 0 };

    DeviceState = DEVICE_STATE_RESTORE;
    printf( "###### ===== ClassA demo application v1.0.0 ==== ######\n\n" );
    while(1)
    {
      // Process Radio IRQ
      if( Radio.IrqProcess != NULL )
      {
          Radio.IrqProcess( );
      }
      // Processes the LoRaMac events
      LoRaMacProcess( );

      switch( DeviceState )
      {
          case DEVICE_STATE_RESTORE:
          {

              // Read secure-element DEV_EUI, JOI_EUI and SE_PIN values.
              mibReq.Type = MIB_DEV_EUI;
              LoRaMacMibGetRequestConfirm( &mibReq );
              memcpy1( devEui, mibReq.Param.DevEui, 8 );

              mibReq.Type = MIB_JOIN_EUI;
              LoRaMacMibGetRequestConfirm( &mibReq );
              memcpy1( joinEui, mibReq.Param.JoinEui, 8 );

              mibReq.Type = MIB_SE_PIN;
              LoRaMacMibGetRequestConfirm( &mibReq );
              memcpy1( sePin, mibReq.Param.SePin, 4 );
#if( OVER_THE_AIR_ACTIVATION == 0 )
              // Tell the MAC layer which network server version are we connecting too.
              mibReq.Type = MIB_ABP_LORAWAN_VERSION;
              mibReq.Param.AbpLrWanVersion.Value = ABP_ACTIVATION_LRWAN_VERSION;
              LoRaMacMibSetRequestConfirm( &mibReq );

              mibReq.Type = MIB_NET_ID;
              mibReq.Param.NetID = LORAWAN_NETWORK_ID;
              LoRaMacMibSetRequestConfirm( &mibReq );

#if( STATIC_DEVICE_ADDRESS != 1 )
               // Random seed initialization
               srand1( BoardGetRandomSeed( ) );
               // Choose a random device address
               DevAddr = randr( 0, 0x01FFFFFF );
#endif

              mibReq.Type = MIB_DEV_ADDR;
              mibReq.Param.DevAddr = DevAddr;
              LoRaMacMibSetRequestConfirm( &mibReq );
#endif // #if( OVER_THE_AIR_ACTIVATION == 0 )
              DeviceState = DEVICE_STATE_START;
              break;
          }

          case DEVICE_STATE_START:
          {
              //TimerInit( &TxNextPacketTimer, OnTxNextPacketTimerEvent );
              TxNextPacketTimer = xTimerCreate("TxNxt",
                                                1,           // Just for initilization. Period updated later in stack, before timer starts
                                                pdFALSE,     // One-shot
                                                (void * ) 0, // Initialize number of times timer has expired (metadata)
                                                OnTxNextPacketTimerEvent); 
              configASSERT(TxNextPacketTimer != NULL);
                                               

              // TimerInit( &Led1Timer, OnLed1TimerEvent );
              //TimerSetValue( &Led1Timer, 25 );

              //TimerInit( &Led2Timer, OnLed2TimerEvent );
              //TimerSetValue( &Led2Timer, 25 );

              mibReq.Type = MIB_PUBLIC_NETWORK;
              mibReq.Param.EnablePublicNetwork = LORAWAN_PUBLIC_NETWORK;
              LoRaMacMibSetRequestConfirm( &mibReq );

              mibReq.Type = MIB_ADR;
              mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
              LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( REGION_EU868 ) || defined( REGION_RU864 ) || defined( REGION_CN779 ) || defined( REGION_EU433 )
              LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
#endif
              mibReq.Type = MIB_SYSTEM_MAX_RX_ERROR;
              mibReq.Param.SystemMaxRxError = 20;
              LoRaMacMibSetRequestConfirm( &mibReq );

              LoRaMacStart( );

              mibReq.Type = MIB_NETWORK_ACTIVATION;
              status = LoRaMacMibGetRequestConfirm( &mibReq );

              if( status == LORAMAC_STATUS_OK )
              {
                  if( mibReq.Param.NetworkActivation == ACTIVATION_TYPE_NONE )
                  {
                      DeviceState = DEVICE_STATE_JOIN;
                  }
                  else
                  {
                      DeviceState = DEVICE_STATE_SEND;
                      NextTx = true;
                  }
              }
              break;
          }
          case DEVICE_STATE_JOIN:
          {
              mibReq.Type = MIB_DEV_EUI;
              LoRaMacMibGetRequestConfirm( &mibReq );
              printf( "DevEui      : %02X", mibReq.Param.DevEui[0] );
              for( int i = 1; i < 8; i++ )
              {
                  printf( "-%02X", mibReq.Param.DevEui[i] );
              }
              printf( "\n" );
              mibReq.Type = MIB_JOIN_EUI;
              LoRaMacMibGetRequestConfirm( &mibReq );
              printf( "JoinEui     : %02X", mibReq.Param.JoinEui[0] );
              for( int i = 1; i < 8; i++ )
              {
                  printf( "-%02X", mibReq.Param.JoinEui[i] );
              }
              printf( "\n" );
              mibReq.Type = MIB_SE_PIN;
              LoRaMacMibGetRequestConfirm( &mibReq );
              printf( "Pin         : %02X", mibReq.Param.SePin[0] );
              for( int i = 1; i < 4; i++ )
              {
                  printf( "-%02X", mibReq.Param.SePin[i] );
              }
              printf( "\n\n" );
#if( OVER_THE_AIR_ACTIVATION == 0 )
              printf( "###### ===== JOINED ==== ######\n" );
              printf( "\nABP\n\n" );
              printf( "DevAddr     : %08lX\n", DevAddr );
              printf( "\n\n" );

              mibReq.Type = MIB_NETWORK_ACTIVATION;
              mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
              LoRaMacMibSetRequestConfirm( &mibReq );

              DeviceState = DEVICE_STATE_SEND;
#else
              JoinNetwork( );
#endif
              break;
          }
          case DEVICE_STATE_SEND:
          {
              if( NextTx == true )
              {
                  PrepareTxFrame( AppPort );

                  NextTx = SendFrame( );
              }
              DeviceState = DEVICE_STATE_CYCLE;
              break;
          }
          case DEVICE_STATE_CYCLE:
          {
              DeviceState = DEVICE_STATE_SLEEP;
              if( ComplianceTest.Running == true )
              {
                  // Schedule next packet transmission
                  TxDutyCycleTime = 5000; // 5000 ms
              }
              else
              {
                  // Schedule next packet transmission
                  TxDutyCycleTime = APP_TX_DUTYCYCLE + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
              }

              // Schedule next packet transmission
              /*
              TimerSetValue( &TxNextPacketTimer, TxDutyCycleTime );
              TimerStart( &TxNextPacketTimer );
              */
              FreeRTOS_TimerSetValue( TxNextPacketTimer, TxDutyCycleTime );
              FreeRTOS_TimerStart( TxNextPacketTimer );
              break;
          }
          case DEVICE_STATE_SLEEP:
          {


              if( IsMacProcessPending == 1 )
              {
                  CRITICAL_SECTION_BEGIN( );
                  // Clear flag and prevent MCU to go into low power modes.
                  IsMacProcessPending = 0;
                  CRITICAL_SECTION_END( );
              }
              else
              {
                  //__WFE();
                  //configASSERT(0);
                  // The MCU wakes up through events
                  //BoardLowPowerHandler( );
              }

              break;
          }
          default:
          {
              DeviceState = DEVICE_STATE_START;
              break;
          }
      }

        // Status aesthetics
        //nrf_gpio_pin_toggle(SX1262_PIN_LED);
        //vTaskDelay(xSleepTick);
        nrf_gpio_pin_toggle(LED_APP_TOGGLE);
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




