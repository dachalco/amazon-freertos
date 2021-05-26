#include <stdarg.h>
#include <string.h>

#include "mcuboot_config/mcuboot_logging.h"

//#include "bsp.h"
#include "boards.h"
#include "app_uart.h"
#include "nrf_peripherals.h"



#if defined( UART_PRESENT )
    #include "nrf_uart.h"
#endif
#if defined( UARTE_PRESENT )
    #include "nrf_uarte.h"
#endif

#define configLOGGING_MAX_MESSAGE_LENGTH 256

void vUartPrint( uint8_t * pucData )
{
    uint32_t xErrCode;

    for( uint32_t i = 0; i < configLOGGING_MAX_MESSAGE_LENGTH; i++ )
    {
        if(pucData[ i ] == 0)
        {
            break;
        }

        do
        {
            xErrCode = app_uart_put(pucData[ i ]);
            if(xErrCode == NRF_ERROR_NO_MEM)
            {
                xErrCode = 0;
            }
        } while( xErrCode == NRF_ERROR_BUSY );
    }
}


void vUartPrintf( uint8_t usLoggingLevel,
                         const char * pcFormat,
                         va_list args )
{
    size_t xLength = 0;
    int32_t xLength2 = 0;


    /* Allocate a buffer to hold the log message. */
    static char pcPrintString[configLOGGING_MAX_MESSAGE_LENGTH] = { 0 };

    if( pcPrintString != NULL )
    {
        const char * pcLevelString = NULL;
        size_t ulFormatLen = 0UL;

        /* Choose the string for the log level metadata for the log message. */
        switch( usLoggingLevel )
        {
            case MCUBOOT_LOG_LEVEL_ERROR:
                pcLevelString = "ERROR";
                break;

            case MCUBOOT_LOG_LEVEL_WARNING:
                pcLevelString = "WARN";
                break;

            case MCUBOOT_LOG_LEVEL_INFO:
                pcLevelString = "INFO";
                break;

            case MCUBOOT_LOG_LEVEL_DEBUG:
                pcLevelString = "DEBUG";
        }

        /* Add the chosen log level information as prefix for the message. */
        if( pcLevelString != NULL )
        {
            xLength += snprintf( pcPrintString + xLength, configLOGGING_MAX_MESSAGE_LENGTH - xLength, "[%s] ", pcLevelString );
        }

        xLength2 = vsnprintf( pcPrintString + xLength, configLOGGING_MAX_MESSAGE_LENGTH - xLength, pcFormat, args );

        if( xLength2 < 0 )
        {
            /* vsnprintf() failed. Restore the terminating NULL
             * character of the first part. Note that the first
             * part of the buffer may be empty if the value of
             * configLOGGING_INCLUDE_TIME_AND_TASK_NAME is not
             * 1 and as a result, the whole buffer may be empty.
             * That's the reason we have a check for xLength > 0
             * before sending the buffer to the logging task.
             */
            xLength2 = 0;
            pcPrintString[ xLength ] = '\0';
        }

        xLength += ( size_t ) xLength2;

        /* Add newline characters if the message does not end with them.*/
        ulFormatLen = strlen( pcFormat );

        if( ( ulFormatLen >= 2 ) && ( strncmp( pcFormat + ulFormatLen, "\r\n", 2 ) != 0 ) )
        {
            xLength += snprintf( pcPrintString + xLength, configLOGGING_MAX_MESSAGE_LENGTH - xLength, "%s", "\r\n" );
        }

        /* Only send the buffer to the logging task if it is
         * not empty. */
        if( xLength > 0 )
        {
            vUartPrint(pcPrintString);
        }
    }

}


void vLogError( const char * pcFormat, ... )
{
    va_list args;
    va_start( args, pcFormat );

    vUartPrintf(MCUBOOT_LOG_LEVEL_ERROR, pcFormat, args );

    va_end( args );

}
void vLogWarning( const char * pcFormat, ... )
{
    va_list args;
    va_start( args, pcFormat );

    vUartPrintf(MCUBOOT_LOG_LEVEL_WARNING, pcFormat, args );

    va_end( args );
}
void vLogInfo( const char * pcFormat, ... )
{
    va_list args;
    va_start( args, pcFormat );

    vUartPrintf(MCUBOOT_LOG_LEVEL_INFO, pcFormat, args );

    va_end( args );
}
void vLogDebug( const char * pcFormat, ... )
{
    va_list args;
    va_start( args, pcFormat );

    vUartPrintf(MCUBOOT_LOG_LEVEL_DEBUG, pcFormat, args );

    va_end( args );
}