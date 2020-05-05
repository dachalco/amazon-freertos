/*!
 * \file      board.c
 *
 * \brief     Target board general functions implementation
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

#include "utilities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board-config.h"

void BoardCriticalSectionBegin( uint32_t *mask )
{
    (void)mask;
    taskENTER_CRITICAL();
}

void BoardCriticalSectionEnd( uint32_t *mask )
{
    (void)mask;
    taskEXIT_CRITICAL();
}

// Strictly for demo purposes, just hardcode some ID to make rest of the stack happy
void BoardGetUniqueId( uint8_t *id )
{
    id[7] = 0xCA;
    id[6] = 0xFE;
    id[5] = 0u;
    id[4] = 0u;
    id[3] = 0u;
    id[2] = 0u;
    id[1] = 0x00;
    id[0] = 0xDC;
}
