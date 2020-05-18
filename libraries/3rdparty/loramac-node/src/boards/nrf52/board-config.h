/*!
 * \file      board-config.h
 *
 * \brief     Board configuration
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
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Daniel Jaeckle ( STACKFORCE )
 *
 * \author    Johannes Bruder ( STACKFORCE )
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "nrf_gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif



#define BOARD_TCXO_WAKEUP_TIME                      0

/*!
 * Board MCU pins definitions
 */

 /* Interfaced pins */
#define RADIO_RESET                                 NRF_GPIO_PIN_MAP(0, 3)  // Out: Active LOW shield reset
#define RADIO_MOSI                                  NRF_GPIO_PIN_MAP(1, 13) // Out: SPI Slave input
#define RADIO_MISO                                  NRF_GPIO_PIN_MAP(1, 14) // In : SPI Slave output
#define RADIO_SCLK                                  NRF_GPIO_PIN_MAP(1, 15) // Out: SPI Slave clock
#define RADIO_NSS                                   NRF_GPIO_PIN_MAP(1, 8)  // Out: SPI Slave select
#define RADIO_BUSY                                  NRF_GPIO_PIN_MAP(1, 4)  // In :
#define RADIO_DIO_1                                 NRF_GPIO_PIN_MAP(1, 6)  // In : Setup to be the IRQ line. Needs GPIOTE

/* Probe pins for different stages of chip setup. See PCB Circuit. Read ONLY */
#define RADIO_ANT_SWITCH_POWER                      NRF_GPIO_PIN_MAP(1, 10)  // In
#define RADIO_FREQ_SEL                              NRF_GPIO_PIN_MAP(0, 4)   // In
#define RADIO_XTAL_SEL                              NRF_GPIO_PIN_MAP(0, 29)  // In
#define RADIO_DEVICE_SEL                            NRF_GPIO_PIN_MAP(0, 28)  // In

/* Does not influence radio chip */
#define LED_1                                       NRF_GPIO_PIN_MAP(0, 14)  // Out: nrf52 onboard LED2. Used for status/sanity check. Does not affect chip


// None of these should be needed for minimal nrf52 demo port
/*
// Debug pins definition.
#define RADIO_DBG_PIN_TX                            PB_6
#define RADIO_DBG_PIN_RX                            PC_7

#define OSC_LSE_IN                                  PC_14
#define OSC_LSE_OUT                                 PC_15

#define OSC_HSE_IN                                  PH_0
#define OSC_HSE_OUT                                 PH_1

#define SWCLK                                       PA_14
#define SWDAT                                       PA_13

#define I2C_SCL                                     PB_8
#define I2C_SDA                                     PB_9

#define UART_TX                                     PA_2
#define UART_RX                                     PA_3
*/

#ifdef __cplusplus
}
#endif

#endif // __BOARD_CONFIG_H__
