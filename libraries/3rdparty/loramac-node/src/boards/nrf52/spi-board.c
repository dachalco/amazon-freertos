/*!
 * \file      spi-board.c
 *
 * \brief     Target board SPI driver implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include "FreeRTOS.h"
#include "utilities.h"
#include "board.h"
#include "board-config.h"
#include "gpio.h"
#include "spi.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"

#define SPI_INSTANCE  0                                           /**< SPI instance index. */
static const nrf_drv_spi_t spi_instance = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static volatile bool spi_xfer_done = true;  /**< Flag used to indicate that SPI instance completed the transfer. */

// Debug metrics
static uint32_t n_transfers = 0;

// Their SPI driver is synchronous, this one resuses nrf's spim APIs which are async. Need to accumulate data then flag when done
void spi_event_handler(nrf_drv_spi_evt_t const * pxEvent, void * pvContext)
{
    n_transfers++;
    spi_xfer_done = true;
}

/* Current demo setup will only allow use of SPI_1. 
   Additionally, there will only be one device in slave chain, so this will not use NSS pin */
void SpiInit( Spi_t *obj, SpiId_t spiId, PinNames mosi, PinNames miso, PinNames sclk, PinNames nss )
{
    configASSERT(obj && spiId == SPI_1);
    //CRITICAL_SECTION_BEGIN( );
    
    // Honor existing metadata
    obj->SpiId = spiId;


    // Configure/Init nrf spi instance
    nrfx_spi_config_t spi_config = NRFX_SPI_DEFAULT_CONFIG;
    spi_config.frequency          = NRF_SPI_FREQ_8M; 
    spi_config.miso_pin           = RADIO_MISO;
    spi_config.mosi_pin           = RADIO_MOSI;
    spi_config.sck_pin            = RADIO_SCLK;
    spi_config.ss_pin             = NRF_DRV_SPI_PIN_NOT_USED;
    configASSERT(NRF_SUCCESS == nrf_drv_spi_init(&spi_instance, &spi_config, spi_event_handler, NULL));


    //CRITICAL_SECTION_END();
}

/*
void SpiDeInit( Spi_t *obj )
{
    HAL_SPI_DeInit( &SpiHandle[obj->SpiId] );

    GpioInit( &obj->Mosi, obj->Mosi.pin, PIN_OUTPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0 );
    GpioInit( &obj->Miso, obj->Miso.pin, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_DOWN, 0 );
    GpioInit( &obj->Sclk, obj->Sclk.pin, PIN_OUTPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0 );
    GpioInit( &obj->Nss, obj->Nss.pin, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1 );
}

void SpiFormat( Spi_t *obj, int8_t bits, int8_t cpol, int8_t cpha, int8_t slave )
{
    SpiHandle[obj->SpiId].Init.Direction = SPI_DIRECTION_2LINES;
    if( bits == SPI_DATASIZE_8BIT )
    {
        SpiHandle[obj->SpiId].Init.DataSize = SPI_DATASIZE_8BIT;
    }
    else
    {
        SpiHandle[obj->SpiId].Init.DataSize = SPI_DATASIZE_16BIT;
    }
    SpiHandle[obj->SpiId].Init.CLKPolarity = cpol;
    SpiHandle[obj->SpiId].Init.CLKPhase = cpha;
    SpiHandle[obj->SpiId].Init.FirstBit = SPI_FIRSTBIT_MSB;
    SpiHandle[obj->SpiId].Init.TIMode = SPI_TIMODE_DISABLE;
    SpiHandle[obj->SpiId].Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    SpiHandle[obj->SpiId].Init.CRCPolynomial = 7;

    if( slave == 0 )
    {
        SpiHandle[obj->SpiId].Init.Mode = SPI_MODE_MASTER;
    }
    else
    {
        SpiHandle[obj->SpiId].Init.Mode = SPI_MODE_SLAVE;
    }
}

void SpiFrequency( Spi_t *obj, uint32_t hz )
{
    uint32_t divisor = 0;
    uint32_t sysClkTmp = SystemCoreClock;
    uint32_t baudRate;

    while( sysClkTmp > hz )
    {
        divisor++;
        sysClkTmp = ( sysClkTmp >> 1 );

        if( divisor >= 7 )
        {
            break;
        }
    }

    baudRate =( ( ( divisor & 0x4 ) == 0 ) ? 0x0 : SPI_CR1_BR_2 ) |
              ( ( ( divisor & 0x2 ) == 0 ) ? 0x0 : SPI_CR1_BR_1 ) |
              ( ( ( divisor & 0x1 ) == 0 ) ? 0x0 : SPI_CR1_BR_0 );

    SpiHandle[obj->SpiId].Init.BaudRatePrescaler = baudRate;
}
*/

uint16_t SpiInOut( Spi_t *obj, uint16_t outData )
{
    configASSERT(obj);
    
    // Configure transaction to send and receive byte.
    uint8_t txByte = outData & 0xFF;
    uint8_t rxByte = 0;
    spi_xfer_done = false;
    configASSERT(NRF_SUCCESS == nrf_drv_spi_transfer(&spi_instance,
                                                     &txByte, sizeof(txByte),
                                                     &rxByte, sizeof(rxByte)));
    while (!spi_xfer_done) 
    {
        __WFE(); // Enter low-power state until event occurs
    }

    return rxByte;
}

