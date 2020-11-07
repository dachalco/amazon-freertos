#if 0
/*
 * FreeRTOS V202007.00
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

/**
 * @file aws_iot_demo_shadow.c
 * @brief Demonstrates usage of the Thing Shadow library.
 *
 * This program demonstrates the using Shadow documents to toggle a state called
 * "powerOn" in a remote device.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"
#include "platform/iot_threads.h"

/* MQTT include. */
#include "iot_mqtt.h"

/* Shadow include. */
#include "aws_iot_shadow.h"

/* JSON utilities include. */
#include "iot_json_utils.h"

/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 *
 * Provide default values for undefined configuration settings.
 */
#ifndef AWS_IOT_DEMO_SHADOW_UPDATE_COUNT
    #define AWS_IOT_DEMO_SHADOW_UPDATE_COUNT        ( 20 )
#endif
#ifndef AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS
    #define AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS    ( 3000 )
#endif
/** @endcond */

/* Validate Shadow demo configuration settings. */
#if AWS_IOT_DEMO_SHADOW_UPDATE_COUNT <= 0
    #error "AWS_IOT_DEMO_SHADOW_UPDATE_COUNT cannot be 0 or negative."
#endif
#if AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS <= 0
    #error "AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS cannot be 0 or negative."
#endif

/**
 * @brief The keep-alive interval used for this demo.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS    ( 60 )

/**
 * @brief The timeout for Shadow and MQTT operations in this demo.
 */
#define TIMEOUT_MS            ( 5000 )

/**
 * @brief Format string representing a Shadow document with a "desired" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_DESIRED_JSON     \
    "{"                         \
    "\"state\":{"               \
    "\"desired\":{"             \
    "\"powerOn\":%01d"          \
    "}"                         \
    "},"                        \
    "\"clientToken\":\"%06lu\"" \
    "}"

/**
 * @brief The expected size of #SHADOW_DESIRED_JSON.
 *
 * Because all the format specifiers in #SHADOW_DESIRED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_DESIRED_JSON_SIZE    ( sizeof( SHADOW_DESIRED_JSON ) - 3 )

/**
 * @brief Format string representing a Shadow document with a "reported" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_REPORTED_JSON    \
    "{"                         \
    "\"state\":{"               \
    "\"reported\":{"            \
    "\"powerOn\":%01d"          \
    "}"                         \
    "},"                        \
    "\"clientToken\":\"%06lu\"" \
    "}"

/**
 * @brief The expected size of #SHADOW_REPORTED_JSON.
 *
 * Because all the format specifiers in #SHADOW_REPORTED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_REPORTED_JSON_SIZE    ( sizeof( SHADOW_REPORTED_JSON ) - 3 )

/*-----------------------------------------------------------*/

/* Declaration of demo function. */
int RunShadowDemo( bool awsIotMqttMode,
                   const char * pIdentifier,
                   void * pNetworkServerInfo,
                   void * pNetworkCredentialInfo,
                   const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/

/**
 * @brief Parses a key in the "state" section of a Shadow delta document.
 *
 * @param[in] pDeltaDocument The Shadow delta document to parse.
 * @param[in] deltaDocumentLength The length of `pDeltaDocument`.
 * @param[in] pDeltaKey The key in the delta document to find. Must be NULL-terminated.
 * @param[out] pDelta Set to the first character in the delta key.
 * @param[out] pDeltaLength The length of the delta key.
 *
 * @return `true` if the given delta key is found; `false` otherwise.
 */
static bool _getDelta( const char * pDeltaDocument,
                       size_t deltaDocumentLength,
                       const char * pDeltaKey,
                       const char ** pDelta,
                       size_t * pDeltaLength )
{
    bool stateFound = false, deltaFound = false;
    const size_t deltaKeyLength = strlen( pDeltaKey );
    const char * pState = NULL;
    size_t stateLength = 0;

    /* Find the "state" key in the delta document. */
    stateFound = IotJsonUtils_FindJsonValue( pDeltaDocument,
                                             deltaDocumentLength,
                                             "state",
                                             5,
                                             &pState,
                                             &stateLength );

    if( stateFound == true )
    {
        /* Find the delta key within the "state" section. */
        deltaFound = IotJsonUtils_FindJsonValue( pState,
                                                 stateLength,
                                                 pDeltaKey,
                                                 deltaKeyLength,
                                                 pDelta,
                                                 pDeltaLength );
    }
    else
    {
        IotLogWarn( "Failed to find \"state\" in Shadow delta document." );
    }

    return deltaFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief Parses the "state" key from the "previous" or "current" sections of a
 * Shadow updated document.
 *
 * @param[in] pUpdatedDocument The Shadow updated document to parse.
 * @param[in] updatedDocumentLength The length of `pUpdatedDocument`.
 * @param[in] pSectionKey Either "previous" or "current". Must be NULL-terminated.
 * @param[out] pState Set to the first character in "state".
 * @param[out] pStateLength Length of the "state" section.
 *
 * @return `true` if the "state" was found; `false` otherwise.
 */
static bool _getUpdatedState( const char * pUpdatedDocument,
                              size_t updatedDocumentLength,
                              const char * pSectionKey,
                              const char ** pState,
                              size_t * pStateLength )
{
    bool sectionFound = false, stateFound = false;
    const size_t sectionKeyLength = strlen( pSectionKey );
    const char * pSection = NULL;
    size_t sectionLength = 0;

    /* Find the given section in the updated document. */
    sectionFound = IotJsonUtils_FindJsonValue( pUpdatedDocument,
                                               updatedDocumentLength,
                                               pSectionKey,
                                               sectionKeyLength,
                                               &pSection,
                                               &sectionLength );

    if( sectionFound == true )
    {
        /* Find the "state" key within the "previous" or "current" section. */
        stateFound = IotJsonUtils_FindJsonValue( pSection,
                                                 sectionLength,
                                                 "state",
                                                 5,
                                                 pState,
                                                 pStateLength );
    }
    else
    {
        IotLogWarn( "Failed to find section %s in Shadow updated document.",
                    pSectionKey );
    }

    return stateFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow delta callback, invoked when the desired and updates Shadow
 * states differ.
 *
 * This function simulates a device updating its state in response to a Shadow.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow delta document.
 */
static void _shadowDeltaCallback( void * pCallbackContext,
                                  AwsIotShadowCallbackParam_t * pCallbackParam )
{
    bool deltaFound = false;
    const char * pDelta = NULL;
    size_t deltaLength = 0;
    IotSemaphore_t * pDeltaSemaphore = pCallbackContext;
    int updateDocumentLength = 0;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    /* Stored state. */
    static int32_t currentState = 0;

    /* A buffer containing the update document. It has static duration to prevent
     * it from being placed on the call stack. */
    static char pUpdateDocument[ EXPECTED_REPORTED_JSON_SIZE + 1 ] = { 0 };

    /* Check if there is a different "powerOn" state in the Shadow. */
    deltaFound = _getDelta( pCallbackParam->u.callback.pDocument,
                            pCallbackParam->u.callback.documentLength,
                            "powerOn",
                            &pDelta,
                            &deltaLength );

    if( deltaFound == true )
    {
        /* Change the current state based on the value in the delta document. */
        if( *pDelta == '0' )
        {
            IotLogInfo( "%.*s changing state from %d to 0.",
                        pCallbackParam->thingNameLength,
                        pCallbackParam->pThingName,
                        currentState );

            currentState = 0;
        }
        else if( *pDelta == '1' )
        {
            IotLogInfo( "%.*s changing state from %d to 1.",
                        pCallbackParam->thingNameLength,
                        pCallbackParam->pThingName,
                        currentState );

            currentState = 1;
        }
        else
        {
            IotLogWarn( "Unknown powerOn state parsed from delta document." );
        }

        /* Set the common members to report the new state. */
        updateDocument.pThingName = pCallbackParam->pThingName;
        updateDocument.thingNameLength = pCallbackParam->thingNameLength;
        updateDocument.u.update.pUpdateDocument = pUpdateDocument;
        updateDocument.u.update.updateDocumentLength = EXPECTED_REPORTED_JSON_SIZE;

        /* Generate a Shadow document for the reported state. To keep the client
         * token within 6 characters, it is modded by 1000000. */
        updateDocumentLength = snprintf( pUpdateDocument,
                                         EXPECTED_REPORTED_JSON_SIZE + 1,
                                         SHADOW_REPORTED_JSON,
                                         ( int ) currentState,
                                         ( long unsigned ) ( IotClock_GetTimeMs() % 1000000 ) );

        if( ( size_t ) updateDocumentLength != EXPECTED_REPORTED_JSON_SIZE )
        {
            IotLogError( "Failed to generate reported state document for Shadow update." );
        }
        else
        {
            /* Send the Shadow update. Its result is not checked, as the Shadow updated
             * callback will report if the Shadow was successfully updated. Because the
             * Shadow is constantly updated in this demo, the "Keep Subscriptions" flag
             * is passed to this function. */
            updateStatus = AwsIotShadow_Update( pCallbackParam->mqttConnection,
                                                &updateDocument,
                                                AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                                NULL,
                                                NULL );

            if( updateStatus != AWS_IOT_SHADOW_STATUS_PENDING )
            {
                IotLogWarn( "%.*s failed to report new state.",
                            pCallbackParam->thingNameLength,
                            pCallbackParam->pThingName );
            }
            else
            {
                IotLogInfo( "%.*s sent new state report.",
                            pCallbackParam->thingNameLength,
                            pCallbackParam->pThingName );
            }
        }
    }
    else
    {
        IotLogWarn( "Failed to parse powerOn state from delta document." );
    }

    /* Post to the delta semaphore to unblock the thread sending Shadow updates. */
    IotSemaphore_Post( pDeltaSemaphore );
}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow updated callback, invoked when the Shadow document changes.
 *
 * This function reports when a Shadow has been updated.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow updated document.
 */
static void _shadowUpdatedCallback( void * pCallbackContext,
                                    AwsIotShadowCallbackParam_t * pCallbackParam )
{
    bool previousFound = false, currentFound = false;
    const char * pPrevious = NULL, * pCurrent = NULL;
    size_t previousLength = 0, currentLength = 0;

    /* Silence warnings about unused parameters. */
    ( void ) pCallbackContext;

    /* Find the previous Shadow document. */
    previousFound = _getUpdatedState( pCallbackParam->u.callback.pDocument,
                                      pCallbackParam->u.callback.documentLength,
                                      "previous",
                                      &pPrevious,
                                      &previousLength );

    /* Find the current Shadow document. */
    currentFound = _getUpdatedState( pCallbackParam->u.callback.pDocument,
                                     pCallbackParam->u.callback.documentLength,
                                     "current",
                                     &pCurrent,
                                     &currentLength );

    /* Log the previous and current states. */
    if( ( previousFound == true ) && ( currentFound == true ) )
    {
        IotLogInfo( "Shadow was updated!\r\n"
                    "Previous: {\"state\":%.*s}\r\n"
                    "Current:  {\"state\":%.*s}",
                    previousLength,
                    pPrevious,
                    currentLength,
                    pCurrent );
    }
    else
    {
        if( previousFound == false )
        {
            IotLogWarn( "Previous state not found in Shadow updated document." );
        }

        if( currentFound == false )
        {
            IotLogWarn( "Current state not found in Shadow updated document." );
        }
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the the MQTT library and the Shadow library.
 *
 * @return `EXIT_SUCCESS` if all libraries were successfully initialized;
 * `EXIT_FAILURE` otherwise.
 */
static int _initializeDemo( void )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t mqttInitStatus = IOT_MQTT_SUCCESS;
    AwsIotShadowError_t shadowInitStatus = AWS_IOT_SHADOW_SUCCESS;

    /* Flags to track cleanup on error. */
    bool mqttInitialized = false;

    /* Initialize the MQTT library. */
    mqttInitStatus = IotMqtt_Init();

    if( mqttInitStatus == IOT_MQTT_SUCCESS )
    {
        mqttInitialized = true;
    }
    else
    {
        status = EXIT_FAILURE;
    }

    /* Initialize the Shadow library. */
    if( status == EXIT_SUCCESS )
    {
        /* Use the default MQTT timeout. */
        shadowInitStatus = AwsIotShadow_Init( 0 );

        if( shadowInitStatus != AWS_IOT_SHADOW_SUCCESS )
        {
            status = EXIT_FAILURE;
        }
    }

    /* Clean up on error. */
    if( status == EXIT_FAILURE )
    {
        if( mqttInitialized == true )
        {
            IotMqtt_Cleanup();
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Clean up the the MQTT library and the Shadow library.
 */
static void _cleanupDemo( void )
{
    AwsIotShadow_Cleanup();
    IotMqtt_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Establish a new connection to the MQTT server for the Shadow demo.
 *
 * @param[in] pIdentifier NULL-terminated MQTT client identifier. The Shadow
 * demo will use the Thing Name as the client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 * @param[out] pMqttConnection Set to the handle to the new MQTT connection.
 *
 * @return `EXIT_SUCCESS` if the connection is successfully established; `EXIT_FAILURE`
 * otherwise.
 */
static int _establishMqttConnection( const char * pIdentifier,
                                     void * pNetworkServerInfo,
                                     void * pNetworkCredentialInfo,
                                     const IotNetworkInterface_t * pNetworkInterface,
                                     IotMqttConnection_t * pMqttConnection )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;

    if( pIdentifier == NULL )
    {
        IotLogError( "Shadow Thing Name must be provided." );

        status = EXIT_FAILURE;
    }

    if( status == EXIT_SUCCESS )
    {
        /* Set the members of the network info not set by the initializer. This
         * struct provided information on the transport layer to the MQTT connection. */
        networkInfo.createNetworkConnection = true;
        networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
        networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
        networkInfo.pNetworkInterface = pNetworkInterface;

        /* Set the members of the connection info not set by the initializer. */
        connectInfo.awsIotMqttMode = true;
        connectInfo.cleanSession = true;
        connectInfo.keepAliveSeconds = KEEP_ALIVE_SECONDS;

        /* AWS IoT recommends the use of the Thing Name as the MQTT client ID. */
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = ( uint16_t ) strlen( pIdentifier );

        IotLogInfo( "Shadow Thing Name is %.*s (length %hu).",
                    connectInfo.clientIdentifierLength,
                    connectInfo.pClientIdentifier,
                    connectInfo.clientIdentifierLength );

        /* Establish the MQTT connection. */
        connectStatus = IotMqtt_Connect( &networkInfo,
                                         &connectInfo,
                                         TIMEOUT_MS,
                                         pMqttConnection );

        if( connectStatus != IOT_MQTT_SUCCESS )
        {
            IotLogError( "MQTT CONNECT returned error %s.",
                         IotMqtt_strerror( connectStatus ) );

            status = EXIT_FAILURE;
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Set the Shadow callback functions used in this demo.
 *
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow callbacks were set; `EXIT_FAILURE`
 * otherwise.
 */
static int _setShadowCallbacks( IotSemaphore_t * pDeltaSemaphore,
                                IotMqttConnection_t mqttConnection,
                                const char * pThingName,
                                size_t thingNameLength )
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t callbackStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowCallbackInfo_t deltaCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER,
                               updatedCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER;

    /* Set the functions for callbacks. */
    deltaCallback.pCallbackContext = pDeltaSemaphore;
    deltaCallback.function = _shadowDeltaCallback;
    updatedCallback.function = _shadowUpdatedCallback;

    /* Set the delta callback, which notifies of different desired and reported
     * Shadow states. */
    callbackStatus = AwsIotShadow_SetDeltaCallback( mqttConnection,
                                                    pThingName,
                                                    thingNameLength,
                                                    0,
                                                    &deltaCallback );

    if( callbackStatus == AWS_IOT_SHADOW_SUCCESS )
    {
        /* Set the updated callback, which notifies when a Shadow document is
         * changed. */
        callbackStatus = AwsIotShadow_SetUpdatedCallback( mqttConnection,
                                                          pThingName,
                                                          thingNameLength,
                                                          0,
                                                          &updatedCallback );
    }

    if( callbackStatus != AWS_IOT_SHADOW_SUCCESS )
    {
        IotLogError( "Failed to set demo shadow callback, error %s.",
                     AwsIotShadow_strerror( callbackStatus ) );

        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Try to delete any Shadow document in the cloud.
 *
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Shadow Thing Name to delete.
 * @param[in] thingNameLength The length of `pThingName`.
 */
static void _clearShadowDocument( IotMqttConnection_t mqttConnection,
                                  const char * const pThingName,
                                  size_t thingNameLength )
{
    AwsIotShadowError_t deleteStatus = AWS_IOT_SHADOW_STATUS_PENDING;

    /* Delete any existing Shadow document so that this demo starts with an empty
     * Shadow. */
    deleteStatus = AwsIotShadow_TimedDelete( mqttConnection,
                                             pThingName,
                                             thingNameLength,
                                             0,
                                             TIMEOUT_MS );

    /* Check for return values of "SUCCESS" and "NOT FOUND". Both of these values
     * mean that the Shadow document is now empty. */
    if( ( deleteStatus == AWS_IOT_SHADOW_SUCCESS ) || ( deleteStatus == AWS_IOT_SHADOW_NOT_FOUND ) )
    {
        IotLogInfo( "Successfully cleared Shadow of %.*s.",
                    thingNameLength,
                    pThingName );
    }
    else
    {
        IotLogWarn( "Shadow of %.*s not cleared.",
                    thingNameLength,
                    pThingName );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Send the Shadow updates that will trigger the Shadow callbacks.
 *
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow updates were sent; `EXIT_FAILURE`
 * otherwise.
 */
static int _sendShadowUpdates( IotSemaphore_t * pDeltaSemaphore,
                               IotMqttConnection_t mqttConnection,
                               const char * const pThingName,
                               size_t thingNameLength )
{
    int status = EXIT_SUCCESS;
    int32_t i = 0, desiredState = 0;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    /* A buffer containing the update document. It has static duration to prevent
     * it from being placed on the call stack. */
    static char pUpdateDocument[ EXPECTED_DESIRED_JSON_SIZE + 1 ] = { 0 };

    /* Set the common members of the Shadow update document info. */
    updateDocument.pThingName = pThingName;
    updateDocument.thingNameLength = thingNameLength;
    updateDocument.u.update.pUpdateDocument = pUpdateDocument;
    updateDocument.u.update.updateDocumentLength = EXPECTED_DESIRED_JSON_SIZE;

    /* Publish Shadow updates at a set period. */
    for( i = 1; i <= AWS_IOT_DEMO_SHADOW_UPDATE_COUNT; i++ )
    {
        /* Toggle the desired state. */
        desiredState = !( desiredState );

        /* Generate a Shadow desired state document, using a timestamp for the client
         * token. To keep the client token within 6 characters, it is modded by 1000000. */
        status = snprintf( pUpdateDocument,
                           EXPECTED_DESIRED_JSON_SIZE + 1,
                           SHADOW_DESIRED_JSON,
                           ( int ) desiredState,
                           ( long unsigned ) ( IotClock_GetTimeMs() % 1000000 ) );

        /* Check for errors from snprintf. The expected value is the length of
         * the desired JSON document less the format specifier for the state. */
        if( ( size_t ) status != EXPECTED_DESIRED_JSON_SIZE )
        {
            IotLogError( "Failed to generate desired state document for Shadow update"
                         " %d of %d.", i, AWS_IOT_DEMO_SHADOW_UPDATE_COUNT );

            status = EXIT_FAILURE;
            break;
        }
        else
        {
            status = EXIT_SUCCESS;
        }

        IotLogInfo( "Sending Shadow update %d of %d: %s",
                    i,
                    AWS_IOT_DEMO_SHADOW_UPDATE_COUNT,
                    pUpdateDocument );

        /* Send the Shadow update. Because the Shadow is constantly updated in
         * this demo, the "Keep Subscriptions" flag is passed to this function.
         * Note that this flag only needs to be passed on the first call, but
         * passing it for subsequent calls is fine.
         */
        updateStatus = AwsIotShadow_TimedUpdate( mqttConnection,
                                                 &updateDocument,
                                                 AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                                 TIMEOUT_MS );

        /* Check the status of the Shadow update. */
        if( updateStatus != AWS_IOT_SHADOW_SUCCESS )
        {
            IotLogError( "Failed to send Shadow update %d of %d, error %s.",
                         i,
                         AWS_IOT_DEMO_SHADOW_UPDATE_COUNT,
                         AwsIotShadow_strerror( updateStatus ) );

            status = EXIT_FAILURE;
            break;
        }
        else
        {
            IotLogInfo( "Successfully sent Shadow update %d of %d.",
                        i,
                        AWS_IOT_DEMO_SHADOW_UPDATE_COUNT );

            /* Wait for the delta callback to change its state before continuing. */
            if( IotSemaphore_TimedWait( pDeltaSemaphore, TIMEOUT_MS ) == false )
            {
                IotLogError( "Timed out waiting on delta callback to change state." );

                status = EXIT_FAILURE;
                break;
            }
        }

        IotClock_SleepMs( AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS );
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief The function that runs the Shadow demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Ignored for the Shadow demo.
 * @param[in] pIdentifier NULL-terminated Shadow Thing Name.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunShadowDemo( bool awsIotMqttMode,
                   const char * pIdentifier,
                   void * pNetworkServerInfo,
                   void * pNetworkCredentialInfo,
                   const IotNetworkInterface_t * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = 0;

    /* Handle of the MQTT connection used in this demo. */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Length of Shadow Thing Name. */
    size_t thingNameLength = 0;

    /* Allows the Shadow update function to wait for the delta callback to complete
     * a state change before continuing. */
    IotSemaphore_t deltaSemaphore;

    /* Flags for tracking which cleanup functions must be called. */
    bool librariesInitialized = false, connectionEstablished = false, deltaSemaphoreCreated = false;

    /* The first parameter of this demo function is not used. Shadows are specific
     * to AWS IoT, so this value is hardcoded to true whenever needed. */
    ( void ) awsIotMqttMode;

    /* Determine the length of the Thing Name. */
    if( pIdentifier != NULL )
    {
        thingNameLength = strlen( pIdentifier );

        if( thingNameLength == 0 )
        {
            IotLogError( "The length of the Thing Name (identifier) must be nonzero." );

            status = EXIT_FAILURE;
        }
    }
    else
    {
        IotLogError( "A Thing Name (identifier) must be provided for the Shadow demo." );

        status = EXIT_FAILURE;
    }

    /* Initialize the libraries required for this demo. */
    if( status == EXIT_SUCCESS )
    {
        status = _initializeDemo();
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the libraries as initialized. */
        librariesInitialized = true;

        /* Establish a new MQTT connection. */
        status = _establishMqttConnection( pIdentifier,
                                           pNetworkServerInfo,
                                           pNetworkCredentialInfo,
                                           pNetworkInterface,
                                           &mqttConnection );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the MQTT connection as established. */
        connectionEstablished = true;

        /* Create the semaphore that synchronizes with the delta callback. */
        deltaSemaphoreCreated = IotSemaphore_Create( &deltaSemaphore, 0, 1 );

        if( deltaSemaphoreCreated == false )
        {
            status = EXIT_FAILURE;
        }
    }

    if( status == EXIT_SUCCESS )
    {
        /* Set the Shadow callbacks for this demo. */
        status = _setShadowCallbacks( &deltaSemaphore,
                                      mqttConnection,
                                      pIdentifier,
                                      thingNameLength );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Clear the Shadow document so that this demo starts with no existing
         * Shadow. */
        _clearShadowDocument( mqttConnection, pIdentifier, thingNameLength );

        /* Send Shadow updates. */
        status = _sendShadowUpdates( &deltaSemaphore,
                                     mqttConnection,
                                     pIdentifier,
                                     thingNameLength );

        /* Delete the Shadow document created by this demo to clean up. */
        _clearShadowDocument( mqttConnection, pIdentifier, thingNameLength );
    }

    /* Disconnect the MQTT connection if it was established. */
    if( connectionEstablished == true )
    {
        IotMqtt_Disconnect( mqttConnection, 0 );
    }

    /* Clean up libraries if they were initialized. */
    if( librariesInitialized == true )
    {
        _cleanupDemo();
    }

    /* Destroy the delta semaphore if it was created. */
    if( deltaSemaphoreCreated == true )
    {
        IotSemaphore_Destroy( &deltaSemaphore );
    }

    return status;
}

/*-----------------------------------------------------------*/
#else
/*
 * FreeRTOS V202007.00
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

/**
 * @file aws_iot_demo_shadow.c
 * @brief Demonstrates usage of the Thing Shadow library.
 *
 * This program demonstrates the using Shadow documents to toggle a state called
 * "powerOn" in a remote device.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set up logging for this demo. */
#include "iot_demo_logging.h"

/* Platform layer includes. */
#include "platform/iot_clock.h"
#include "platform/iot_threads.h"

/* MQTT include. */
#include "iot_mqtt.h"

/* Shadow include. */
#include "aws_iot_shadow.h"

/* JSON utilities include. */
#include "iot_json_utils.h"

uint8_t UART4_rxBuffer[111] = { 0xAA };
uint8_t UART4_reportBuffer[111] = { 0xDC };

/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 *
 * Provide default values for undefined configuration settings.
 */
#ifndef AWS_IOT_DEMO_SHADOW_UPDATE_COUNT
#define AWS_IOT_DEMO_SHADOW_UPDATE_COUNT        ( 1 )
#endif
#ifndef AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS
#define AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS    ( 10000 )
#endif
/** @endcond */

/* Validate Shadow demo configuration settings. */
#if AWS_IOT_DEMO_SHADOW_UPDATE_COUNT <= 0
#error "AWS_IOT_DEMO_SHADOW_UPDATE_COUNT cannot be 0 or negative."
#endif
#if AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS <= 0
#error "AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS cannot be 0 or negative."
#endif

/**
 * @brief The keep-alive interval used for this demo.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS    ( 300 )

/**
 * @brief The timeout for Shadow and MQTT operations in this demo.
 */
#define TIMEOUT_MS            ( 15000 )

/**
 * @brief Format string representing a Shadow document with a "desired" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_DESIRED_JSON     \
	"{"                         \
	"\"state\":{"               \
		"\"desired\":{"             \
			"\"ALARMS\":{"             \
				"\"alarms1Hi\":%01u,"       \
				"\"alarms1Lo\":%01u,"       \
				"\"alarms1MuteHi\":%01u,"   \
				"\"alarms1MuteLo\":%01u,"   \
				"\"alarms1AndHi\":%01u,"    \
				"\"alarms1AndLo\":%01u,"    \
				"\"alarms1DiffHi\":%01u,"   \
				"\"alarms1DiffLo\":%01u,"   \
				"\"alarms2Hi\":%01u,"       \
				"\"alarms2Lo\":%01u,"       \
				"\"alarms2MuteHi\":%01u,"   \
				"\"alarms2MuteLo\":%01u,"   \
				"\"alarms2AndHi\":%01u,"    \
				"\"alarms2AndLo\":%01u,"    \
				"\"alarms2DiffHi\":%01u,"   \
				"\"alarms2DiffLo\":%01u"    \
				"},"                             \
			"\"PUMPINFO\":{"                \
				"\"availPumps\":%01u,"       \
				"\"p1avail\":%01u,"          \
				"\"p1availDip\":%01u,"       \
				"\"p1On\":%01u,"             \
				"\"p1Auto\":%01u,"           \
				"\"p1Run\":%01u,"            \
				"\"p1Therm\":%01u,"          \
				"\"p1Seal\":%01u,"           \
				"\"p1Fail\":%01u,"           \
				"\"p1Alarm\":%01u,"          \
				"\"p1Pos\":%01u,"            \
				"\"p1Status\":%01u,"         \
				"\"p2avail\":%01u,"          \
				"\"p2availDip\":%01u,"       \
				"\"p2On\":%01u,"             \
				"\"p2Auto\":%01u,"           \
				"\"p2Run\":%01u,"            \
				"\"p2Therm\":%01u,"          \
				"\"p2Seal\":%01u,"           \
				"\"p2Fail\":%01u,"           \
				"\"p2Alarm\":%01u,"          \
				"\"p2Pos\":%01u,"            \
				"\"p2Status\":%01u,"         \
				"\"p3avail\":%01u,"          \
				"\"p3availDip\":%01u,"       \
				"\"p3On\":%01u,"             \
				"\"p3Auto\":%01u,"           \
				"\"p3Run\":%01u,"            \
				"\"p3Therm\":%01u,"          \
				"\"p3Seal\":%01u,"           \
				"\"p3Fail\":%01u,"           \
				"\"p3Alarm\":%01u,"          \
				"\"p3Pos\":%01u,"            \
				"\"p3Status\":%01u,"         \
				"\"p4avail\":%01u,"          \
				"\"p4availDip\":%01u,"       \
				"\"p4On\":%01u,"             \
				"\"p4Auto\":%01u,"           \
				"\"p4Run\":%01u,"            \
				"\"p4Therm\":%01u,"          \
				"\"p4Seal\":%01u,"           \
				"\"p4Fail\":%01u,"           \
				"\"p4Alarm\":%01u,"          \
				"\"p4Pos\":%01u,"            \
				"\"p4Status\":%01u,"         \
				"\"pStage\":%01u"            \
				"},"                         \
			"\"OPTIONS\":{"                  \
				"\"modeHi\":%01u,"           \
				"\"modeLo\":%01u,"           \
				"\"optionsHi\":%01u,"        \
				"\"optionsLo\":%01u,"        \
				"\"altOff\":%01u,"           \
				"\"altMode\":%01u,"          \
				"\"altBits\":%01u,"          \
				"\"rtcl1\":%01u,"            \
				"\"rtcl2\":%01u,"            \
				"\"rtclStat\":%01u,"         \
				"\"forceOff\":%01u,"         \
				"\"jockOn\":%01u,"           \
				"\"emergOn\":%01u,"          \
				"\"cellOn\":%01u,"           \
				"\"lowOn\":%01u,"            \
				"\"highOn\":%01u,"           \
				"\"altFloats\":%01u,"        \
				"\"numPumps\":%01u"          \
				"},"                         \
			"\"FLOATS\":{"                   \
				"\"badFloats\":%01u,"        \
				"\"badRegFloats\":%01u,"     \
				"\"commFloats\":%01u,"       \
				"\"stuckOpenFloats\":%01u,"  \
				"\"stuckOpenClosed\":%01u,"  \
				"\"badLowFloat\":%01u,"      \
				"\"badHighFloat\":%01u,"     \
				"\"lowFloatOn\":%01u,"       \
				"\"offFloatOn\":%01u,"       \
				"\"leadFloatOn\":%01u,"      \
				"\"lag1FloatOn\":%01u,"      \
				"\"lag2FloatOn\":%01u,"      \
				"\"lag3FloatOn\":%01u,"      \
				"\"highFloatOn\":%01u"       \
				"},"                         \
			"\"LEVELS\":{"                   \
				"\"s1LevelHi\":%01u,"        \
				"\"s1LevelLo\":%01u,"        \
				"\"s2LevelHi\":%01u,"        \
				"\"s2LevelLo\":%01u,"        \
				"\"s3LevelHi\":%01u,"        \
				"\"s3LevelLo\":%01u,"        \
				"\"s4LevelHi\":%01u,"        \
				"\"s4LevelLo\":%01u,"        \
				"\"s5LevelHi\":%01u,"        \
				"\"s5LevelLo\":%01u"         \
				"},"                         \
			"\"RELAYS\":{"                   \
				"\"relay1\":%01u,"           \
				"\"relay2\":%01u,"           \
				"\"relay3\":%01u,"           \
				"\"relay4\":%01u,"           \
				"\"relay5\":%01u,"           \
				"\"audAlarm\":%01u,"         \
				"\"comnAlarm\":%01u"         \
				"}"                          \
		"}"                         		 \
	"},"                        			 \
	"\"clientToken\":\"%06lu\"" 			 \
	"}"

/**
 * @brief The expected size of #SHADOW_DESIRED_JSON.
 *
 * Because all the format specifiers in #SHADOW_DESIRED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_DESIRED_JSON_SIZE    ( sizeof( SHADOW_DESIRED_JSON ) - 3 )

/**
 * @brief Format string representing a Shadow document with a "reported" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_REPORTED_JSON    \
    "{"                         \
    "\"state\":{"               \
    	"\"reported\":{"            \
			"\"ALARMS\":{"             \
				"\"alarms1Hi\":%01u,"       \
				"\"alarms1Lo\":%01u,"       \
				"\"alarms1MuteHi\":%01u,"   \
				"\"alarms1MuteLo\":%01u,"   \
				"\"alarms1AndHi\":%01u,"    \
				"\"alarms1AndLo\":%01u,"    \
				"\"alarms1DiffHi\":%01u,"   \
				"\"alarms1DiffLo\":%01u,"   \
				"\"alarms2Hi\":%01u,"       \
				"\"alarms2Lo\":%01u,"       \
				"\"alarms2MuteHi\":%01u,"   \
				"\"alarms2MuteLo\":%01u,"   \
				"\"alarms2AndHi\":%01u,"    \
				"\"alarms2AndLo\":%01u,"    \
				"\"alarms2DiffHi\":%01u,"   \
				"\"alarms2DiffLo\":%01u"    \
				"},"                             \
			"\"PUMPINFO\":{"                \
				"\"availPumps\":%01u,"       \
				"\"p1avail\":%01u,"          \
				"\"p1availDip\":%01u,"       \
				"\"p1On\":%01u,"             \
				"\"p1Auto\":%01u,"           \
				"\"p1Run\":%01u,"            \
				"\"p1Therm\":%01u,"          \
				"\"p1Seal\":%01u,"           \
				"\"p1Fail\":%01u,"           \
				"\"p1Alarm\":%01u,"          \
				"\"p1Pos\":%01u,"            \
				"\"p1Status\":%01u,"         \
				"\"p2avail\":%01u,"          \
				"\"p2availDip\":%01u,"       \
				"\"p2On\":%01u,"             \
				"\"p2Auto\":%01u,"           \
				"\"p2Run\":%01u,"            \
				"\"p2Therm\":%01u,"          \
				"\"p2Seal\":%01u,"           \
				"\"p2Fail\":%01u,"           \
				"\"p2Alarm\":%01u,"          \
				"\"p2Pos\":%01u,"            \
				"\"p2Status\":%01u,"         \
				"\"p3avail\":%01u,"          \
				"\"p3availDip\":%01u,"       \
				"\"p3On\":%01u,"             \
				"\"p3Auto\":%01u,"           \
				"\"p3Run\":%01u,"            \
				"\"p3Therm\":%01u,"          \
				"\"p3Seal\":%01u,"           \
				"\"p3Fail\":%01u,"           \
				"\"p3Alarm\":%01u,"          \
				"\"p3Pos\":%01u,"            \
				"\"p3Status\":%01u,"         \
				"\"p4avail\":%01u,"          \
				"\"p4availDip\":%01u,"       \
				"\"p4On\":%01u,"             \
				"\"p4Auto\":%01u,"           \
				"\"p4Run\":%01u,"            \
				"\"p4Therm\":%01u,"          \
				"\"p4Seal\":%01u,"           \
				"\"p4Fail\":%01u,"           \
				"\"p4Alarm\":%01u,"          \
				"\"p4Pos\":%01u,"            \
				"\"p4Status\":%01u,"         \
				"\"pStage\":%01u"            \
				"},"                         \
			"\"OPTIONS\":{"                  \
				"\"modeHi\":%01u,"           \
				"\"modeLo\":%01u,"           \
				"\"optionsHi\":%01u,"        \
				"\"optionsLo\":%01u,"        \
				"\"altOff\":%01u,"           \
				"\"altMode\":%01u,"          \
				"\"altBits\":%01u,"          \
				"\"rtcl1\":%01u,"            \
				"\"rtcl2\":%01u,"            \
				"\"rtclStat\":%01u,"         \
				"\"forceOff\":%01u,"         \
				"\"jockOn\":%01u,"           \
				"\"emergOn\":%01u,"          \
				"\"cellOn\":%01u,"           \
				"\"lowOn\":%01u,"            \
				"\"highOn\":%01u,"           \
				"\"altFloats\":%01u,"        \
				"\"numPumps\":%01u"          \
				"},"                         \
			"\"FLOATS\":{"                   \
				"\"badFloats\":%01u,"        \
				"\"badRegFloats\":%01u,"     \
				"\"commFloats\":%01u,"       \
				"\"stuckOpenFloats\":%01u,"  \
				"\"stuckOpenClosed\":%01u,"  \
				"\"badLowFloat\":%01u,"      \
				"\"badHighFloat\":%01u,"     \
				"\"lowFloatOn\":%01u,"       \
				"\"offFloatOn\":%01u,"       \
				"\"leadFloatOn\":%01u,"      \
				"\"lag1FloatOn\":%01u,"      \
				"\"lag2FloatOn\":%01u,"      \
				"\"lag3FloatOn\":%01u,"      \
				"\"highFloatOn\":%01u"       \
				"},"                         \
			"\"LEVELS\":{"                   \
				"\"s1LevelHi\":%01u,"        \
				"\"s1LevelLo\":%01u,"        \
				"\"s2LevelHi\":%01u,"        \
				"\"s2LevelLo\":%01u,"        \
				"\"s3LevelHi\":%01u,"        \
				"\"s3LevelLo\":%01u,"        \
				"\"s4LevelHi\":%01u,"        \
				"\"s4LevelLo\":%01u,"        \
				"\"s5LevelHi\":%01u,"        \
				"\"s5LevelLo\":%01u"         \
				"},"                         \
			"\"RELAYS\":{"                   \
				"\"relay1\":%01u,"           \
				"\"relay2\":%01u,"           \
				"\"relay3\":%01u,"           \
				"\"relay4\":%01u,"           \
				"\"relay5\":%01u,"           \
				"\"audAlarm\":%01u,"         \
				"\"comnAlarm\":%01u"         \
				"}"                          \
		"}"                         		 \
	"},"                        			 \
	"\"clientToken\":\"%06lu\"" 			 \
	"}"

/**
 * @brief The expected size of #SHADOW_REPORTED_JSON.
 *
 * Because all the format specifiers in #SHADOW_REPORTED_JSON include a length,
 * its full size is known at compile-time.
 */
#define EXPECTED_REPORTED_JSON_SIZE    ( sizeof( SHADOW_REPORTED_JSON ) - 3 )

/*-----------------------------------------------------------*/

/* Declaration of demo function. */
int RunShadowDemo( bool awsIotMqttMode,
                   const char * pIdentifier,
                   void * pNetworkServerInfo,
                   void * pNetworkCredentialInfo,
                   const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/

/**
 * @brief Parses a key in the "state" section of a Shadow delta document.
 *
 * @param[in] pDeltaDocument The Shadow delta document to parse.
 * @param[in] deltaDocumentLength The length of `pDeltaDocument`.
 * @param[in] pDeltaKey The key in the delta document to find. Must be NULL-terminated.
 * @param[out] pDelta Set to the first character in the delta key.
 * @param[out] pDeltaLength The length of the delta key.
 *
 * @return `true` if the given delta key is found; `false` otherwise.
 */
static bool _getDelta( const char * pDeltaDocument,
                       size_t deltaDocumentLength,
                       const char * pDeltaKey,
                       const char ** pDelta,
                       size_t * pDeltaLength )
{
    bool stateFound = false, deltaFound = false;
    const size_t deltaKeyLength = strlen( pDeltaKey );
    const char * pState = NULL;
    size_t stateLength = 0;

    /* Find the "state" key in the delta document. */
    stateFound = IotJsonUtils_FindJsonValue( pDeltaDocument,
                                             deltaDocumentLength,
                                             "state",
                                             5,
                                             &pState,
                                             &stateLength );

    if( stateFound == true )
    {
        /* Find the delta key within the "state" section. */
        deltaFound = IotJsonUtils_FindJsonValue( pState,
                                                 stateLength,
                                                 pDeltaKey,
                                                 deltaKeyLength,
                                                 pDelta,
                                                 pDeltaLength );
    }
    else
    {
        IotLogWarn( "Failed to find \"state\" in Shadow delta document." );
    }

    return deltaFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief Parses the "state" key from the "previous" or "current" sections of a
 * Shadow updated document.
 *
 * @param[in] pUpdatedDocument The Shadow updated document to parse.
 * @param[in] updatedDocumentLength The length of `pUpdatedDocument`.
 * @param[in] pSectionKey Either "previous" or "current". Must be NULL-terminated.
 * @param[out] pState Set to the first character in "state".
 * @param[out] pStateLength Length of the "state" section.
 *
 * @return `true` if the "state" was found; `false` otherwise.
 */
static bool _getUpdatedState( const char * pUpdatedDocument,
                              size_t updatedDocumentLength,
                              const char * pSectionKey,
                              const char ** pState,
                              size_t * pStateLength )
{
    bool sectionFound = false, stateFound = false;
    const size_t sectionKeyLength = strlen( pSectionKey );
    const char * pSection = NULL;
    size_t sectionLength = 0;

    /* Find the given section in the updated document. */
    sectionFound = IotJsonUtils_FindJsonValue( pUpdatedDocument,
                                               updatedDocumentLength,
                                               pSectionKey,
                                               sectionKeyLength,
                                               &pSection,
                                               &sectionLength );

    if( sectionFound == true )
    {
        /* Find the "state" key within the "previous" or "current" section. */
        stateFound = IotJsonUtils_FindJsonValue( pSection,
                                                 sectionLength,
                                                 "state",
                                                 5,
                                                 pState,
                                                 pStateLength );
    }
    else
    {
        IotLogWarn( "Failed to find section %s in Shadow updated document.",
                    pSectionKey );
    }

    return stateFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow delta callback, invoked when the desired and updates Shadow
 * states differ.
 *
 * This function simulates a device updating its state in response to a Shadow.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow delta document.
 */
static void _shadowDeltaCallback( void * pCallbackContext,
                                  AwsIotShadowCallbackParam_t * pCallbackParam )
{
    bool deltaFound = false;
    const char * pDelta = NULL;
    size_t deltaLength = 0;
    IotSemaphore_t * pDeltaSemaphore = pCallbackContext;
    int updateDocumentLength = 0;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    /* Stored state. */
    static int32_t currentState = 0;

    /* A buffer containing the update document. It has static duration to prevent
     * it from being placed on the call stack. */
    static char pUpdateDocument[ EXPECTED_REPORTED_JSON_SIZE + 1 ] = { 0 };

    /* Check if there is a different "powerOn" state in the Shadow. */
    deltaFound = _getDelta( pCallbackParam->u.callback.pDocument,
                            pCallbackParam->u.callback.documentLength,
                            "alarms1Lo",
                            &pDelta,
                            &deltaLength );

    if( deltaFound == true )
    {
        /* Change the current state based on the value in the delta document. */
        if( *pDelta == '0' )
        {
            IotLogInfo( "%.*s changing state from %d to 0.",
                        pCallbackParam->thingNameLength,
                        pCallbackParam->pThingName,
                        currentState );

            currentState = 0;
        }
        else if( *pDelta == '1' )
        {
            IotLogInfo( "%.*s changing state from %d to 1.",
                        pCallbackParam->thingNameLength,
                        pCallbackParam->pThingName,
                        currentState );

            currentState = 1;
        }
        else
        {
            IotLogWarn( "Unknown powerOn state parsed from delta document." );
        }

        /* Set the common members to report the new state. */
        updateDocument.pThingName = pCallbackParam->pThingName;
        updateDocument.thingNameLength = pCallbackParam->thingNameLength;
        updateDocument.u.update.pUpdateDocument = pUpdateDocument;
        updateDocument.u.update.updateDocumentLength = EXPECTED_REPORTED_JSON_SIZE;

        /* Generate a Shadow document for the reported state. To keep the client
         * token within 6 characters, it is modded by 1000000. */
        updateDocumentLength = snprintf( pUpdateDocument,
                                         EXPECTED_REPORTED_JSON_SIZE + 1,
                                         SHADOW_REPORTED_JSON,
                                         ( unsigned int ) UART4_rxBuffer[0],
                                         ( unsigned int ) UART4_rxBuffer[1],
                                         ( unsigned int ) UART4_rxBuffer[2],
                                         ( unsigned int ) UART4_rxBuffer[3],
                                         ( unsigned int ) UART4_rxBuffer[4],
                                         ( unsigned int ) UART4_rxBuffer[5],
                                         ( unsigned int ) UART4_rxBuffer[6],
                                         ( unsigned int ) UART4_rxBuffer[7],
                                         ( unsigned int ) UART4_rxBuffer[8],
                                         ( unsigned int ) UART4_rxBuffer[9],
                                         ( unsigned int ) UART4_rxBuffer[10],
                                         ( unsigned int ) UART4_rxBuffer[11],
                                         ( unsigned int ) UART4_rxBuffer[12],
                                         ( unsigned int ) UART4_rxBuffer[13],
                                         ( unsigned int ) UART4_rxBuffer[14],
                                         ( unsigned int ) UART4_rxBuffer[15],
                                         ( unsigned int ) UART4_rxBuffer[16],
                                         ( unsigned int ) UART4_rxBuffer[17],
                                         ( unsigned int ) UART4_rxBuffer[18],
                                         ( unsigned int ) UART4_rxBuffer[19],
                                         ( unsigned int ) UART4_rxBuffer[20],
                                         ( unsigned int ) UART4_rxBuffer[21],
                                         ( unsigned int ) UART4_rxBuffer[22],
                                         ( unsigned int ) UART4_rxBuffer[23],
                                         ( unsigned int ) UART4_rxBuffer[24],
                                         ( unsigned int ) UART4_rxBuffer[25],
                                         ( unsigned int ) UART4_rxBuffer[26],
                                         ( unsigned int ) UART4_rxBuffer[27],
                                         ( unsigned int ) UART4_rxBuffer[28],
                                         ( unsigned int ) UART4_rxBuffer[29],
                                         ( unsigned int ) UART4_rxBuffer[30],
                                         ( unsigned int ) UART4_rxBuffer[31],
                                         ( unsigned int ) UART4_rxBuffer[32],
                                         ( unsigned int ) UART4_rxBuffer[33],
                                         ( unsigned int ) UART4_rxBuffer[34],
                                         ( unsigned int ) UART4_rxBuffer[35],
                                         ( unsigned int ) UART4_rxBuffer[36],
                                         ( unsigned int ) UART4_rxBuffer[37],
                                         ( unsigned int ) UART4_rxBuffer[38],
                                         ( unsigned int ) UART4_rxBuffer[39],
                                         ( unsigned int ) UART4_rxBuffer[40],
                                         ( unsigned int ) UART4_rxBuffer[41],
                                         ( unsigned int ) UART4_rxBuffer[42],
                                         ( unsigned int ) UART4_rxBuffer[43],
                                         ( unsigned int ) UART4_rxBuffer[44],
                                         ( unsigned int ) UART4_rxBuffer[45],
                                         ( unsigned int ) UART4_rxBuffer[46],
                                         ( unsigned int ) UART4_rxBuffer[47],
                                         ( unsigned int ) UART4_rxBuffer[48],
                                         ( unsigned int ) UART4_rxBuffer[49],
                                         ( unsigned int ) UART4_rxBuffer[50],
                                         ( unsigned int ) UART4_rxBuffer[51],
                                         ( unsigned int ) UART4_rxBuffer[52],
                                         ( unsigned int ) UART4_rxBuffer[53],
                                         ( unsigned int ) UART4_rxBuffer[54],
                                         ( unsigned int ) UART4_rxBuffer[55],
                                         ( unsigned int ) UART4_rxBuffer[56],
                                         ( unsigned int ) UART4_rxBuffer[57],
                                         ( unsigned int ) UART4_rxBuffer[58],
                                         ( unsigned int ) UART4_rxBuffer[59],
                                         ( unsigned int ) UART4_rxBuffer[60],
                                         ( unsigned int ) UART4_rxBuffer[61],
                                         ( unsigned int ) UART4_rxBuffer[62],
                                         ( unsigned int ) UART4_rxBuffer[63],
                                         ( unsigned int ) UART4_rxBuffer[64],
                                         ( unsigned int ) UART4_rxBuffer[65],
                                         ( unsigned int ) UART4_rxBuffer[66],
                                         ( unsigned int ) UART4_rxBuffer[67],
                                         ( unsigned int ) UART4_rxBuffer[68],
                                         ( unsigned int ) UART4_rxBuffer[69],
                                         ( unsigned int ) UART4_rxBuffer[70],
                                         ( unsigned int ) UART4_rxBuffer[71],
                                         ( unsigned int ) UART4_rxBuffer[72],
                                         ( unsigned int ) UART4_rxBuffer[73],
                                         ( unsigned int ) UART4_rxBuffer[74],
                                         ( unsigned int ) UART4_rxBuffer[75],
                                         ( unsigned int ) UART4_rxBuffer[76],
                                         ( unsigned int ) UART4_rxBuffer[77],
                                         ( unsigned int ) UART4_rxBuffer[78],
                                         ( unsigned int ) UART4_rxBuffer[79],
                                         ( unsigned int ) UART4_rxBuffer[80],
                                         ( unsigned int ) UART4_rxBuffer[81],
                                         ( unsigned int ) UART4_rxBuffer[82],
                                         ( unsigned int ) UART4_rxBuffer[83],
                                         ( unsigned int ) UART4_rxBuffer[84],
                                         ( unsigned int ) UART4_rxBuffer[85],
                                         ( unsigned int ) UART4_rxBuffer[86],
                                         ( unsigned int ) UART4_rxBuffer[87],
                                         ( unsigned int ) UART4_rxBuffer[88],
                                         ( unsigned int ) UART4_rxBuffer[89],
                                         ( unsigned int ) UART4_rxBuffer[90],
                                         ( unsigned int ) UART4_rxBuffer[91],
                                         ( unsigned int ) UART4_rxBuffer[92],
                                         ( unsigned int ) UART4_rxBuffer[93],
                                         ( unsigned int ) UART4_rxBuffer[94],
                                         ( unsigned int ) UART4_rxBuffer[95],
                                         ( unsigned int ) UART4_rxBuffer[96],
                                         ( unsigned int ) UART4_rxBuffer[97],
                                         ( unsigned int ) UART4_rxBuffer[98],
                                         ( unsigned int ) UART4_rxBuffer[99],
                                         ( unsigned int ) UART4_rxBuffer[100],
                                         ( unsigned int ) UART4_rxBuffer[101],
                                         ( unsigned int ) UART4_rxBuffer[102],
                                         ( unsigned int ) UART4_rxBuffer[103],
                                         ( unsigned int ) UART4_rxBuffer[104],
                                         ( unsigned int ) UART4_rxBuffer[105],
                                         ( unsigned int ) UART4_rxBuffer[106],
                                         ( unsigned int ) UART4_rxBuffer[107],
                                         ( unsigned int ) UART4_rxBuffer[108],
                                         ( unsigned int ) UART4_rxBuffer[109],
                                         ( unsigned int ) UART4_rxBuffer[110],
                                         ( long unsigned ) ( IotClock_GetTimeMs() % 1000000 ) );

        if( ( size_t ) updateDocumentLength != EXPECTED_REPORTED_JSON_SIZE )
        {
            IotLogError( "Failed to generate reported state document for Shadow update." );
        }
        else
        {
            configPRINTF( ( "%s\n", pUpdateDocument) );
            /* Send the Shadow update. Its result is not checked, as the Shadow updated
             * callback will report if the Shadow was successfully updated. Because the
             * Shadow is constantly updated in this demo, the "Keep Subscriptions" flag
             * is passed to this function. */
            updateStatus = AwsIotShadow_Update( pCallbackParam->mqttConnection,
                                                &updateDocument,
                                                AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                                NULL,
                                                NULL );

            if( updateStatus != AWS_IOT_SHADOW_STATUS_PENDING )
            {
                IotLogWarn( "%.*s failed to report new state.",
                            pCallbackParam->thingNameLength,
                            pCallbackParam->pThingName );
            }
            else
            {
                IotLogInfo( "%.*s sent new state report.",
                            pCallbackParam->thingNameLength,
                            pCallbackParam->pThingName );
            }
        }
    }
    else
    {
        IotLogWarn( "Failed to parse powerOn state from delta document." );
    }

    /* Post to the delta semaphore to unblock the thread sending Shadow updates. */
    IotSemaphore_Post( pDeltaSemaphore );
}

/*-----------------------------------------------------------*/

/**
 * @brief Shadow updated callback, invoked when the Shadow document changes.
 *
 * This function reports when a Shadow has been updated.
 *
 * @param[in] pCallbackContext Not used.
 * @param[in] pCallbackParam The received Shadow updated document.
 */
static void _shadowUpdatedCallback( void * pCallbackContext,
                                    AwsIotShadowCallbackParam_t * pCallbackParam )
{
    bool previousFound = false, currentFound = false;
    const char * pPrevious = NULL, * pCurrent = NULL;
    size_t previousLength = 0, currentLength = 0;

    /* Silence warnings about unused parameters. */
    ( void ) pCallbackContext;

    /* Find the previous Shadow document. */
    previousFound = _getUpdatedState( pCallbackParam->u.callback.pDocument,
                                      pCallbackParam->u.callback.documentLength,
                                      "previous",
                                      &pPrevious,
                                      &previousLength );

    /* Find the current Shadow document. */
    currentFound = _getUpdatedState( pCallbackParam->u.callback.pDocument,
                                     pCallbackParam->u.callback.documentLength,
                                     "current",
                                     &pCurrent,
                                     &currentLength );

    /* Log the previous and current states. */
    if( ( previousFound == true ) && ( currentFound == true ) )
    {
        IotLogInfo( "Shadow was updated!\r\n"
                    "Previous: {\"state\":%.*s}\r\n"
                    "Current:  {\"state\":%.*s}",
                    previousLength,
                    pPrevious,
                    currentLength,
                    pCurrent );
    }
    else
    {
        if( previousFound == false )
        {
            IotLogWarn( "Previous state not found in Shadow updated document." );
        }

        if( currentFound == false )
        {
            IotLogWarn( "Current state not found in Shadow updated document." );
        }
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the the MQTT library and the Shadow library.
 *
 * @return `EXIT_SUCCESS` if all libraries were successfully initialized;
 * `EXIT_FAILURE` otherwise.
 */
static int _initializeDemo( void )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t mqttInitStatus = IOT_MQTT_SUCCESS;
    AwsIotShadowError_t shadowInitStatus = AWS_IOT_SHADOW_SUCCESS;

    /* Flags to track cleanup on error. */
    bool mqttInitialized = false;

    /* Initialize the MQTT library. */
    mqttInitStatus = IotMqtt_Init();

    if( mqttInitStatus == IOT_MQTT_SUCCESS )
    {
        mqttInitialized = true;
    }
    else
    {
        status = EXIT_FAILURE;
    }

    /* Initialize the Shadow library. */
    if( status == EXIT_SUCCESS )
    {
        /* Use the default MQTT timeout. */
        shadowInitStatus = AwsIotShadow_Init( 0 );

        if( shadowInitStatus != AWS_IOT_SHADOW_SUCCESS )
        {
            status = EXIT_FAILURE;
        }
    }

    /* Clean up on error. */
    if( status == EXIT_FAILURE )
    {
        if( mqttInitialized == true )
        {
            IotMqtt_Cleanup();
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Clean up the the MQTT library and the Shadow library.
 */
static void _cleanupDemo( void )
{
    AwsIotShadow_Cleanup();
    IotMqtt_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Establish a new connection to the MQTT server for the Shadow demo.
 *
 * @param[in] pIdentifier NULL-terminated MQTT client identifier. The Shadow
 * demo will use the Thing Name as the client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 * @param[out] pMqttConnection Set to the handle to the new MQTT connection.
 *
 * @return `EXIT_SUCCESS` if the connection is successfully established; `EXIT_FAILURE`
 * otherwise.
 */
static int _establishMqttConnection( const char * pIdentifier,
                                     void * pNetworkServerInfo,
                                     void * pNetworkCredentialInfo,
                                     const IotNetworkInterface_t * pNetworkInterface,
                                     IotMqttConnection_t * pMqttConnection )
{
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;

    if( pIdentifier == NULL )
    {
        IotLogError( "Shadow Thing Name must be provided." );

        status = EXIT_FAILURE;
    }

    if( status == EXIT_SUCCESS )
    {
        /* Set the members of the network info not set by the initializer. This
         * struct provided information on the transport layer to the MQTT connection. */
        networkInfo.createNetworkConnection = true;
        networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
        networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
        networkInfo.pNetworkInterface = pNetworkInterface;

#if ( IOT_MQTT_ENABLE_SERIALIZER_OVERRIDES == 1 ) && defined( IOT_DEMO_MQTT_SERIALIZER )
        networkInfo.pMqttSerializer = IOT_DEMO_MQTT_SERIALIZER;
#endif

        /* Set the members of the connection info not set by the initializer. */
        connectInfo.awsIotMqttMode = true;
        connectInfo.cleanSession = true;
        connectInfo.keepAliveSeconds = KEEP_ALIVE_SECONDS;

        /* AWS IoT recommends the use of the Thing Name as the MQTT client ID. */
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = ( uint16_t ) strlen( pIdentifier );

        IotLogInfo( "Shadow Thing Name is %.*s (length %hu).",
                    connectInfo.clientIdentifierLength,
                    connectInfo.pClientIdentifier,
                    connectInfo.clientIdentifierLength );

        /* Establish the MQTT connection. */
        connectStatus = IotMqtt_Connect( &networkInfo,
                                         &connectInfo,
                                         TIMEOUT_MS,
                                         pMqttConnection );

        if( connectStatus != IOT_MQTT_SUCCESS )
        {
            IotLogError( "MQTT CONNECT returned error %s.",
                         IotMqtt_strerror( connectStatus ) );

            status = EXIT_FAILURE;
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Set the Shadow callback functions used in this demo.
 *
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow callbacks were set; `EXIT_FAILURE`
 * otherwise.
 */
static int _setShadowCallbacks( IotSemaphore_t * pDeltaSemaphore,
                                IotMqttConnection_t mqttConnection,
                                const char * pThingName,
                                size_t thingNameLength )
{
    int status = EXIT_SUCCESS;
    AwsIotShadowError_t callbackStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowCallbackInfo_t deltaCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER,
            updatedCallback = AWS_IOT_SHADOW_CALLBACK_INFO_INITIALIZER;

    /* Set the functions for callbacks. */
    deltaCallback.pCallbackContext = pDeltaSemaphore;
    deltaCallback.function = _shadowDeltaCallback;
    updatedCallback.function = _shadowUpdatedCallback;

    /* Set the delta callback, which notifies of different desired and reported
     * Shadow states. */
    callbackStatus = AwsIotShadow_SetDeltaCallback( mqttConnection,
                                                    pThingName,
                                                    thingNameLength,
                                                    0,
                                                    &deltaCallback );

    if( callbackStatus == AWS_IOT_SHADOW_SUCCESS )
    {
        /* Set the updated callback, which notifies when a Shadow document is
         * changed. */
        callbackStatus = AwsIotShadow_SetUpdatedCallback( mqttConnection,
                                                          pThingName,
                                                          thingNameLength,
                                                          0,
                                                          &updatedCallback );
    }

    if( callbackStatus != AWS_IOT_SHADOW_SUCCESS )
    {
        IotLogError( "Failed to set demo shadow callback, error %s.",
                     AwsIotShadow_strerror( callbackStatus ) );

        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Try to delete any Shadow document in the cloud.
 *
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Shadow Thing Name to delete.
 * @param[in] thingNameLength The length of `pThingName`.
 */
static void _clearShadowDocument( IotMqttConnection_t mqttConnection,
                                  const char * const pThingName,
                                  size_t thingNameLength )
{
    AwsIotShadowError_t deleteStatus = AWS_IOT_SHADOW_STATUS_PENDING;

    /* Delete any existing Shadow document so that this demo starts with an empty
     * Shadow. */
    deleteStatus = AwsIotShadow_TimedDelete( mqttConnection,
                                             pThingName,
                                             thingNameLength,
                                             0,
                                             TIMEOUT_MS );

    /* Check for return values of "SUCCESS" and "NOT FOUND". Both of these values
     * mean that the Shadow document is now empty. */
    if( ( deleteStatus == AWS_IOT_SHADOW_SUCCESS ) || ( deleteStatus == AWS_IOT_SHADOW_NOT_FOUND ) )
    {
        IotLogInfo( "Successfully cleared Shadow of %.*s.",
                    thingNameLength,
                    pThingName );
    }
    else
    {
        IotLogWarn( "Shadow of %.*s not cleared.",
                    thingNameLength,
                    pThingName );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Send the Shadow updates that will trigger the Shadow callbacks.
 *
 * @param[in] pDeltaSemaphore Used to synchronize Shadow updates with the delta
 * callback.
 * @param[in] mqttConnection The MQTT connection used for Shadows.
 * @param[in] pThingName The Thing Name for Shadows in this demo.
 * @param[in] thingNameLength The length of `pThingName`.
 *
 * @return `EXIT_SUCCESS` if all Shadow updates were sent; `EXIT_FAILURE`
 * otherwise.
 */
static int _sendShadowUpdates( IotSemaphore_t * pDeltaSemaphore,
                               IotMqttConnection_t mqttConnection,
                               const char * const pThingName,
                               size_t thingNameLength )
{
    int status = EXIT_SUCCESS;
    int32_t i = 0, desiredState = 0;
    AwsIotShadowError_t updateStatus = AWS_IOT_SHADOW_STATUS_PENDING;
    AwsIotShadowDocumentInfo_t updateDocument = AWS_IOT_SHADOW_DOCUMENT_INFO_INITIALIZER;

    /* A buffer containing the update document. It has static duration to prevent
     * it from being placed on the call stack. */
    //static char pUpdateDocument[ EXPECTED_DESIRED_JSON_SIZE + 1 ] = { 0 };
    static char pUpdateDocument[ EXPECTED_REPORTED_JSON_SIZE + 1 ] = { 0 };

    /* Set the common members of the Shadow update document info. */
    updateDocument.pThingName = pThingName;
    updateDocument.thingNameLength = thingNameLength;
    updateDocument.u.update.pUpdateDocument = pUpdateDocument;
    //updateDocument.u.update.updateDocumentLength = EXPECTED_DESIRED_JSON_SIZE;
    updateDocument.u.update.updateDocumentLength = EXPECTED_REPORTED_JSON_SIZE;

    /* Publish Shadow updates at a set period. */
    for( i = 1; i <= AWS_IOT_DEMO_SHADOW_UPDATE_COUNT; i++ )
    {
        /* Toggle the desired state. */
        //desiredState = !( desiredState );

        /* Generate a Shadow desired state document, using a timestamp for the client
         * token. To keep the client token within 6 characters, it is modded by 1000000. */
        /*
    	status = snprintf( pUpdateDocument,
                           EXPECTED_DESIRED_JSON_SIZE + 1,
                           SHADOW_DESIRED_JSON,
                           ( int ) desiredState,
                           ( long unsigned ) ( IotClock_GetTimeMs() % 1000000 ) );
		*/
        status = snprintf( pUpdateDocument,
                           EXPECTED_REPORTED_JSON_SIZE + 1,
                           SHADOW_REPORTED_JSON,
                           ( unsigned int ) UART4_reportBuffer[0],
                           ( unsigned int ) UART4_rxBuffer[1],
                           ( unsigned int ) UART4_rxBuffer[2],
                           ( unsigned int ) UART4_rxBuffer[3],
                           ( unsigned int ) UART4_rxBuffer[4],
                           ( unsigned int ) UART4_rxBuffer[5],
                           ( unsigned int ) UART4_rxBuffer[6],
                           ( unsigned int ) UART4_rxBuffer[7],
                           ( unsigned int ) UART4_rxBuffer[8],
                           ( unsigned int ) UART4_rxBuffer[9],
                           ( unsigned int ) UART4_rxBuffer[10],
                           ( unsigned int ) UART4_rxBuffer[11],
                           ( unsigned int ) UART4_rxBuffer[12],
                           ( unsigned int ) UART4_rxBuffer[13],
                           ( unsigned int ) UART4_rxBuffer[14],
                           ( unsigned int ) UART4_rxBuffer[15],
                           ( unsigned int ) UART4_rxBuffer[16],
                           ( unsigned int ) UART4_rxBuffer[17],
                           ( unsigned int ) UART4_rxBuffer[18],
                           ( unsigned int ) UART4_rxBuffer[19],
                           ( unsigned int ) UART4_rxBuffer[20],
                           ( unsigned int ) UART4_rxBuffer[21],
                           ( unsigned int ) UART4_rxBuffer[22],
                           ( unsigned int ) UART4_rxBuffer[23],
                           ( unsigned int ) UART4_rxBuffer[24],
                           ( unsigned int ) UART4_rxBuffer[25],
                           ( unsigned int ) UART4_rxBuffer[26],
                           ( unsigned int ) UART4_rxBuffer[27],
                           ( unsigned int ) UART4_rxBuffer[28],
                           ( unsigned int ) UART4_rxBuffer[29],
                           ( unsigned int ) UART4_rxBuffer[30],
                           ( unsigned int ) UART4_rxBuffer[31],
                           ( unsigned int ) UART4_rxBuffer[32],
                           ( unsigned int ) UART4_rxBuffer[33],
                           ( unsigned int ) UART4_rxBuffer[34],
                           ( unsigned int ) UART4_rxBuffer[35],
                           ( unsigned int ) UART4_rxBuffer[36],
                           ( unsigned int ) UART4_rxBuffer[37],
                           ( unsigned int ) UART4_rxBuffer[38],
                           ( unsigned int ) UART4_rxBuffer[39],
                           ( unsigned int ) UART4_rxBuffer[40],
                           ( unsigned int ) UART4_rxBuffer[41],
                           ( unsigned int ) UART4_rxBuffer[42],
                           ( unsigned int ) UART4_rxBuffer[43],
                           ( unsigned int ) UART4_rxBuffer[44],
                           ( unsigned int ) UART4_rxBuffer[45],
                           ( unsigned int ) UART4_rxBuffer[46],
                           ( unsigned int ) UART4_rxBuffer[47],
                           ( unsigned int ) UART4_rxBuffer[48],
                           ( unsigned int ) UART4_rxBuffer[49],
                           ( unsigned int ) UART4_rxBuffer[50],
                           ( unsigned int ) UART4_rxBuffer[51],
                           ( unsigned int ) UART4_rxBuffer[52],
                           ( unsigned int ) UART4_rxBuffer[53],
                           ( unsigned int ) UART4_rxBuffer[54],
                           ( unsigned int ) UART4_rxBuffer[55],
                           ( unsigned int ) UART4_rxBuffer[56],
                           ( unsigned int ) UART4_rxBuffer[57],
                           ( unsigned int ) UART4_rxBuffer[58],
                           ( unsigned int ) UART4_rxBuffer[59],
                           ( unsigned int ) UART4_rxBuffer[60],
                           ( unsigned int ) UART4_rxBuffer[61],
                           ( unsigned int ) UART4_rxBuffer[62],
                           ( unsigned int ) UART4_rxBuffer[63],
                           ( unsigned int ) UART4_rxBuffer[64],
                           ( unsigned int ) UART4_rxBuffer[65],
                           ( unsigned int ) UART4_rxBuffer[66],
                           ( unsigned int ) UART4_rxBuffer[67],
                           ( unsigned int ) UART4_rxBuffer[68],
                           ( unsigned int ) UART4_rxBuffer[69],
                           ( unsigned int ) UART4_rxBuffer[70],
                           ( unsigned int ) UART4_rxBuffer[71],
                           ( unsigned int ) UART4_rxBuffer[72],
                           ( unsigned int ) UART4_rxBuffer[73],
                           ( unsigned int ) UART4_rxBuffer[74],
                           ( unsigned int ) UART4_rxBuffer[75],
                           ( unsigned int ) UART4_rxBuffer[76],
                           ( unsigned int ) UART4_rxBuffer[77],
                           ( unsigned int ) UART4_rxBuffer[78],
                           ( unsigned int ) UART4_rxBuffer[79],
                           ( unsigned int ) UART4_rxBuffer[80],
                           ( unsigned int ) UART4_rxBuffer[81],
                           ( unsigned int ) UART4_rxBuffer[82],
                           ( unsigned int ) UART4_rxBuffer[83],
                           ( unsigned int ) UART4_rxBuffer[84],
                           ( unsigned int ) UART4_rxBuffer[85],
                           ( unsigned int ) UART4_rxBuffer[86],
                           ( unsigned int ) UART4_rxBuffer[87],
                           ( unsigned int ) UART4_rxBuffer[88],
                           ( unsigned int ) UART4_rxBuffer[89],
                           ( unsigned int ) UART4_rxBuffer[90],
                           ( unsigned int ) UART4_rxBuffer[91],
                           ( unsigned int ) UART4_rxBuffer[92],
                           ( unsigned int ) UART4_rxBuffer[93],
                           ( unsigned int ) UART4_rxBuffer[94],
                           ( unsigned int ) UART4_rxBuffer[95],
                           ( unsigned int ) UART4_rxBuffer[96],
                           ( unsigned int ) UART4_rxBuffer[97],
                           ( unsigned int ) UART4_rxBuffer[98],
                           ( unsigned int ) UART4_rxBuffer[99],
                           ( unsigned int ) UART4_rxBuffer[100],
                           ( unsigned int ) UART4_rxBuffer[101],
                           ( unsigned int ) UART4_rxBuffer[102],
                           ( unsigned int ) UART4_rxBuffer[103],
                           ( unsigned int ) UART4_rxBuffer[104],
                           ( unsigned int ) UART4_rxBuffer[105],
                           ( unsigned int ) UART4_rxBuffer[106],
                           ( unsigned int ) UART4_rxBuffer[107],
                           ( unsigned int ) UART4_rxBuffer[108],
                           ( unsigned int ) UART4_rxBuffer[109],
                           ( unsigned int ) UART4_rxBuffer[110],
                           ( long unsigned ) ( IotClock_GetTimeMs() % 1000000 ) );

        /* Check for errors from snprintf. The expected value is the length of
         * the desired JSON document less the format specifier for the state. */
        //if( ( size_t ) status != EXPECTED_DESIRED_JSON_SIZE )
        //if( ( unsigned int ) status != EXPECTED_REPORTED_JSON_SIZE )
        //  {
        //     IotLogError( "Failed to generate desired state document for Shadow update"
        //                  " %d of %d.", i, AWS_IOT_DEMO_SHADOW_UPDATE_COUNT );

        //WB ADDED
        IotLogError( "Status: %d, Expected: %d", status, EXPECTED_REPORTED_JSON_SIZE);


        //     status = EXIT_FAILURE;
        //     break;
        // }
        // else
        // {
        status = EXIT_SUCCESS;
        // }

        IotLogInfo( "Sending Shadow update %d of %d: %s",
                    i,
                    AWS_IOT_DEMO_SHADOW_UPDATE_COUNT,
                    pUpdateDocument );

        /* Send the Shadow update. Because the Shadow is constantly updated in
         * this demo, the "Keep Subscriptions" flag is passed to this function.
         * Note that this flag only needs to be passed on the first call, but
         * passing it for subsequent calls is fine.
         */
        updateStatus = AwsIotShadow_TimedUpdate( mqttConnection,
                                                 &updateDocument,
                                                 AWS_IOT_SHADOW_FLAG_KEEP_SUBSCRIPTIONS,
                                                 TIMEOUT_MS );

        /* Check the status of the Shadow update. */
        if( updateStatus != AWS_IOT_SHADOW_SUCCESS )
        {
            IotLogError( "Failed to send Shadow update %d of %d, error %s.",
                         i,
                         AWS_IOT_DEMO_SHADOW_UPDATE_COUNT,
                         AwsIotShadow_strerror( updateStatus ) );

            status = EXIT_FAILURE;
            break;
        }
        else
        {
            IotLogInfo( "Successfully sent Shadow update %d of %d.",
                        i,
                        AWS_IOT_DEMO_SHADOW_UPDATE_COUNT );

            /* Wait for the delta callback to change its state before continuing. */
            if( IotSemaphore_TimedWait( pDeltaSemaphore, TIMEOUT_MS ) == false )
            {
                IotLogError( "Timed out waiting on delta callback to change state." );

                status = EXIT_FAILURE;
                break;
            }
        }

        IotClock_SleepMs( AWS_IOT_DEMO_SHADOW_UPDATE_PERIOD_MS );
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief The function that runs the Shadow demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Ignored for the Shadow demo.
 * @param[in] pIdentifier NULL-terminated Shadow Thing Name.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection for Shadows.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunShadowDemo( bool awsIotMqttMode,
                   const char * pIdentifier,
                   void * pNetworkServerInfo,
                   void * pNetworkCredentialInfo,
                   const IotNetworkInterface_t * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = 0;

    /* Handle of the MQTT connection used in this demo. */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Length of Shadow Thing Name. */
    size_t thingNameLength = 0;

    /* Allows the Shadow update function to wait for the delta callback to complete
     * a state change before continuing. */
    IotSemaphore_t deltaSemaphore;

    /* Flags for tracking which cleanup functions must be called. */
    bool librariesInitialized = false, connectionEstablished = false, deltaSemaphoreCreated = false;

    /* The first parameter of this demo function is not used. Shadows are specific
     * to AWS IoT, so this value is hardcoded to true whenever needed. */
    ( void ) awsIotMqttMode;

    /* Determine the length of the Thing Name. */
    if( pIdentifier != NULL )
    {
        thingNameLength = strlen( pIdentifier );

        if( thingNameLength == 0 )
        {
            IotLogError( "The length of the Thing Name (identifier) must be nonzero." );

            status = EXIT_FAILURE;
        }
    }
    else
    {
        IotLogError( "A Thing Name (identifier) must be provided for the Shadow demo." );

        status = EXIT_FAILURE;
    }

    /* Initialize the libraries required for this demo. */
    if( status == EXIT_SUCCESS )
    {
        status = _initializeDemo();
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the libraries as initialized. */
        librariesInitialized = true;

        /* Establish a new MQTT connection. */
        status = _establishMqttConnection( pIdentifier,
                                           pNetworkServerInfo,
                                           pNetworkCredentialInfo,
                                           pNetworkInterface,
                                           &mqttConnection );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Mark the MQTT connection as established. */
        connectionEstablished = true;

        /* Create the semaphore that synchronizes with the delta callback. */
        deltaSemaphoreCreated = IotSemaphore_Create( &deltaSemaphore, 0, 1 );

        if( deltaSemaphoreCreated == false )
        {
            status = EXIT_FAILURE;
        }
    }

    if( status == EXIT_SUCCESS )
    {
        /* Set the Shadow callbacks for this demo. */
        status = _setShadowCallbacks( &deltaSemaphore,
                                      mqttConnection,
                                      pIdentifier,
                                      thingNameLength );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Clear the Shadow document so that this demo starts with no existing
         * Shadow. */
        //_clearShadowDocument( mqttConnection, pIdentifier, thingNameLength );

        /* Send Shadow updates. */
        status = _sendShadowUpdates( &deltaSemaphore,
                                     mqttConnection,
                                     pIdentifier,
                                     thingNameLength );

        /* Delete the Shadow document created by this demo to clean up. */
        //_clearShadowDocument( mqttConnection, pIdentifier, thingNameLength );
    }

    /* Disconnect the MQTT connection if it was established. */
    if( connectionEstablished == true )
    {
        IotMqtt_Disconnect( mqttConnection, 0 );
    }

    /* Clean up libraries if they were initialized. */
    if( librariesInitialized == true )
    {
        _cleanupDemo();
    }

    /* Destroy the delta semaphore if it was created. */
    if( deltaSemaphoreCreated == true )
    {
        IotSemaphore_Destroy( &deltaSemaphore );
    }

    return status;
}

/*-----------------------------------------------------------*/
#endif
