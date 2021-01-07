/*
 * FreeRTOS+CLI V1.0.4
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_CLI_Console.h"


#define cmdDEFAULT_TASK_STACK_SIZE  ( configMINIMAL_STACK_SIZE * 4 )

#define cmdDEFAULT_TASK_PRIORITY    ( tskIDLE_PRIORITY + 5 )

#define cmdMAX_INPUT_BUFFER_SIZE    50

#define cmdMAX_ERROR_SIZE           50

/* DEL acts as a backspace. */
#define cmdASCII_DEL                ( 0x7F )

#define cmdERROR_DELAY              ( 1000 )

/* Const messages output by the command console. */
static const char * const pcStartMessage = "\r\n[Input a command or type help then press ENTER to get started]\r\n> ";
static const char * const pcEndOfOutputMessage = "\r\n[Press Enter to run the previous command]\r\n> ";
static const char * const pcNewLine = "\r\n";

static char cInputBuffer[ cmdMAX_INPUT_BUFFER_SIZE ] = "";
static char cErrorString[ cmdMAX_ERROR_SIZE ] = "";

static uint8_t ucCommandIndex = 0;

static void processInputBuffer( xConsoleIO_t consoleIO,
                                int32_t inputSize,
                                char * pCommandBuffer,
                                size_t commandBufferLength,
                                char * pOutputBuffer,
                                size_t outpuBufferLength );


void FreeRTOS_CLIEnterConsoleLoop( xConsoleIO_t consoleIO,
                                   char * pCommandBuffer,
                                   size_t commandBufferLength,
                                   char * pOutputBuffer,
                                   size_t outputBufferLength )
{
    int32_t bytesRead;

    configASSERT( ( consoleIO.read ) && ( consoleIO.write ) );
    configASSERT( ( pCommandBuffer ) && ( commandBufferLength > 0 ) );
    configASSERT( ( pOutputBuffer ) && ( outputBufferLength > 0 ) );

    memset( pCommandBuffer, 0x00, commandBufferLength );
    memset( pOutputBuffer, 0x00, outputBufferLength );
    consoleIO.xMutexIO = xSemaphoreCreateMutex();
    configASSERT(consoleIO.xMutexIO);

    consoleIO.write( pcStartMessage, strlen( pcStartMessage ) );

    for( ; ; )
    {
        /* Read characters to input buffer. */
        bytesRead = consoleIO.read( cInputBuffer, cmdMAX_INPUT_BUFFER_SIZE - 1 );

        if( bytesRead > 0 )
        {
            processInputBuffer( consoleIO,
                                bytesRead,
                                pCommandBuffer,
                                commandBufferLength,
                                pOutputBuffer,
                                outputBufferLength );

            /* Reset input buffer for next iteration. */
            memset( cInputBuffer, 0x00, cmdMAX_INPUT_BUFFER_SIZE );
        }
        else if( bytesRead < 0 )
        {
            snprintf( cErrorString, cmdMAX_ERROR_SIZE, "Read failed with error %d\n", ( int ) bytesRead );
            consoleIO.write( cErrorString, sizeof( cErrorString ) );
            memset( cErrorString, 0x00, cmdMAX_ERROR_SIZE );

            vTaskDelay( pdMS_TO_TICKS( cmdERROR_DELAY ) );
        }
    }
}

typedef struct {
    xConsoleIO_t xConsoleIO;
    CLI_Command_Definition_t * pxCommandDefinition;
    char * pcCMDLine;
    size_t xLengthCMDLine;
    char * pcOutputBuffer;
    size_t xCapacityOutput;
} FreeRTOS_CLICommand_TaskArgs_t;

static void FreeRTOS_CLICommand_Task( void * pvParameters )
{
    BaseType_t xReturned = pdTRUE;
    uint32_t ulNotification = 0;
    FreeRTOS_CLICommand_TaskArgs_t * pxArgs = (FreeRTOS_CLICommand_TaskArgs_t *)pvParameters;

    /* First gain control of console stream. If the console is unavailable, skip the command */
    if( xTaskNotifyWait(0u, UINT32_MAX, &ulNotification, portMAX_DELAY)
        && ulNotification == COMMAND_START 
        && xSemaphoreTake(pxArgs->xConsoleIO.xMutexIO, portMAX_DELAY) )
    {
        do
        {
            if (xTaskNotifyWait(0u, UINT32_MAX, &ulNotification, pdMS_TO_TICKS(30)))
            {
                /* Notification was received. Process any control notifications */
                switch( ulNotification )
                {
                    case COMMAND_CANCEL:
                        pxArgs->xConsoleIO.write("^C", 2);
                        xReturned = pdFALSE;
                        break;

                    default:
                        break;
                }
            }
            else
            {                
                /* Flush output from the command */
                xReturned = pxArgs->pxCommandDefinition->pxCommandInterpreter(pxArgs->pcOutputBuffer, pxArgs->xCapacityOutput, pxArgs->pcCMDLine);
                pxArgs->xConsoleIO.write( pxArgs->pcOutputBuffer, strlen( pxArgs->pcOutputBuffer ) );
                memset( pxArgs->pcOutputBuffer, 0x00, pxArgs->xCapacityOutput );
            }
        } while( xReturned != pdFALSE );

        /* Relinquish control of console stream */
        pxArgs->xConsoleIO.write( pcEndOfOutputMessage, strlen( pcEndOfOutputMessage ) );
        xSemaphoreGive(pxArgs->xConsoleIO.xMutexIO);
    }

    /* Cleanup */
    vPortFree(pxArgs->pcCMDLine);
    vPortFree(pxArgs);
    vTaskDelete( NULL );
}

static void processInputBuffer( xConsoleIO_t consoleIO,
                                int32_t inputSize,
                                char * pCommandBuffer,
                                size_t commandBufferLength,
                                char * pOutputBuffer,
                                size_t outpuBufferLength )
{
    BaseType_t xReturned;
    uint8_t i;
    char cRxedChar;

    for( i = 0; i < inputSize; i++ )
    {
        cRxedChar = cInputBuffer[ i ];

        /* A character was entered.  Add it to the string entered so
         * far. Add a null termination to the end of the string.
         * When a \n is entered the complete string will be
         * passed to the command interpreter. */
        if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) )
        {
            consoleIO.write( &cRxedChar, 1 );

            if( ucCommandIndex < ( commandBufferLength - 1UL ) )
            {
                pCommandBuffer[ ucCommandIndex ] = cRxedChar;
                ucCommandIndex++;
                pCommandBuffer[ ucCommandIndex ] = '\0';
            }
        }
        else if( ( cRxedChar == '\b' ) || ( cRxedChar == cmdASCII_DEL ) )
        {
            /* Backspace was pressed.  Erase the last character in the string - if any. */
            if( ucCommandIndex > 0 )
            {
                consoleIO.write("\b \b", 3);
                ucCommandIndex--;
                pCommandBuffer[ ucCommandIndex ] = '\0';
            }             
        }
        else if( cRxedChar == '\e' )
        {
            /* Escape and control sequences. e.g. arrow_up */
            char ctrl_seq[3] = { 0 };
            consoleIO.read(&ctrl_seq, 2);

            if(ctrl_seq[0] == '[')
            {
                /* Arrow keys. Ignore for now */
                consoleIO.read(&ctrl_seq[2], 1);
            }
        }
        else if ( cRxedChar == 0x03 )
        {
            /* CONTROL+C */
            /* If a command process currently owns console stream, notify the task to stop and terminate */
            /* Use API to get mutex owner which gives task handle. Then notify that task to stop and terminate */
            TaskHandle_t xForegroundCommand = xSemaphoreGetMutexHolder( consoleIO.xMutexIO );
            if( xForegroundCommand )
            { 
                xTaskNotify(xForegroundCommand, COMMAND_CANCEL, eSetValueWithoutOverwrite);
            }
        }
        /* Was it the end of the line? */
        else if( ( cRxedChar == '\n' ) || ( cRxedChar == '\r' ) )
        {
            consoleIO.write( &cRxedChar, 1 );

            /* Skip subsequent '\n', '\r' or '\n' of CRLF. */
            if( ( i > 0 ) &&
                ( ( cInputBuffer[ i - 1 ] == '\r' ) || ( cInputBuffer[ i - 1 ] == '\n' ) ) )
            {
                continue;
            }

            /* Just to space the output from the input. */
            consoleIO.write( pcNewLine, strlen( pcNewLine ) );

            /* For valid commands, spawn a command task */
            CLI_Command_Definition_t * pxCommand = FreeRTOS_CLIFindCommand( pCommandBuffer );
            if( pxCommand )
            {
                /* Spawn the process and allow it to run in parallel so the console can continue to parse inputs.
                * Give the process ownership of console stream.*/
                FreeRTOS_CLICommand_TaskArgs_t * pxCommandArgs = pvPortMalloc(sizeof(FreeRTOS_CLICommand_TaskArgs_t));
                configASSERT(pxCommandArgs);
                
                /* Copy the args so they can persist until command tasks ingests them, then frees them*/
                pxCommandArgs->xConsoleIO = consoleIO;
                pxCommandArgs->pxCommandDefinition = pxCommand;
                pxCommandArgs->xLengthCMDLine = strlen( pCommandBuffer );
                pxCommandArgs->pcCMDLine = pvPortMalloc(pxCommandArgs->xLengthCMDLine + 1 );
                configASSERT(pxCommandArgs->pcCMDLine);
                memcpy(pxCommandArgs->pcCMDLine, pCommandBuffer, pxCommandArgs->xLengthCMDLine + 1);
                pxCommandArgs->pcOutputBuffer = pOutputBuffer;
                pxCommandArgs->xCapacityOutput = outpuBufferLength;

                TaskHandle_t xCommandTaskHandle = NULL;
                xTaskCreate(FreeRTOS_CLICommand_Task,
                            "cmd",
                            cmdDEFAULT_TASK_STACK_SIZE,
                            (void*)pxCommandArgs,
                            cmdDEFAULT_TASK_PRIORITY,
                            &xCommandTaskHandle);

                /* Notify the task that it can take over console stream */
                xTaskNotify(xCommandTaskHandle, COMMAND_START, eSetValueWithoutOverwrite);
                portYIELD();
            }
            else
            {
                consoleIO.write( pcEndOfOutputMessage, strlen( pcEndOfOutputMessage ) );
            }
            

            /* All the strings generated by the input command have been
             * sent.  Clear the command index to receive a new command.
             * Remember the command that was just processed first in case it is
             * to be processed again. */
            ucCommandIndex = 0;
        }
    }
}
