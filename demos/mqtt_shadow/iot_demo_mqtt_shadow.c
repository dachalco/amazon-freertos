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

/**
 * @file iot_demo_mqtt_shadow.c
 * @brief Demonstrates usage of the MQTT library alongside SHADOW library.
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

/*-----------------------------------------------------------*/
/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 *
 * Provide default values for undefined configuration settings.
 */
#ifndef IOT_DEMO_MQTT_TOPIC_PREFIX
    #define IOT_DEMO_MQTT_TOPIC_PREFIX           "iotdemo"
#endif
#ifndef IOT_DEMO_MQTT_PUBLISH_BURST_SIZE
    #define IOT_DEMO_MQTT_PUBLISH_BURST_SIZE     ( 10 )
#endif
#ifndef IOT_DEMO_MQTT_PUBLISH_BURST_COUNT
    #define IOT_DEMO_MQTT_PUBLISH_BURST_COUNT    ( 10 )
#endif
/** @endcond */

/* Validate MQTT demo configuration settings. */
#if IOT_DEMO_MQTT_PUBLISH_BURST_SIZE <= 0
    #error "IOT_DEMO_MQTT_PUBLISH_BURST_SIZE cannot be 0 or negative."
#endif
#if IOT_DEMO_MQTT_PUBLISH_BURST_COUNT <= 0
    #error "IOT_DEMO_MQTT_PUBLISH_BURST_COUNT cannot be 0 or negative."
#endif

/**
 * @brief The first characters in the client identifier. A timestamp is appended
 * to this prefix to create a unique client identifer.
 *
 * This prefix is also used to generate topic names and topic filters used in this
 * demo.
 */
#define CLIENT_IDENTIFIER_PREFIX                 "iotdemo"

/**
 * @brief The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. Add 1 to include the length of the NULL
 * terminator.
 */
#define CLIENT_IDENTIFIER_MAX_LENGTH             ( 24 )

/**
 * @brief The keep-alive interval used for this demo.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#define KEEP_ALIVE_SECONDS                       ( 60 )

/**
 * @brief The Last Will and Testament topic name in this demo.
 *
 * The MQTT server will publish a message to this topic name if this client is
 * unexpectedly disconnected.
 */
#define WILL_TOPIC_NAME                          IOT_DEMO_MQTT_TOPIC_PREFIX "/will"

/**
 * @brief The length of #WILL_TOPIC_NAME.
 */
#define WILL_TOPIC_NAME_LENGTH                   ( ( uint16_t ) ( sizeof( WILL_TOPIC_NAME ) - 1 ) )

/**
 * @brief The message to publish to #WILL_TOPIC_NAME.
 */
#define WILL_MESSAGE                             "MQTT demo unexpectedly disconnected."

/**
 * @brief The length of #WILL_MESSAGE.
 */
#define WILL_MESSAGE_LENGTH                      ( ( size_t ) ( sizeof( WILL_MESSAGE ) - 1 ) )

/**
 * @brief How many topic filters will be used in this demo.
 */
#define TOPIC_FILTER_COUNT                       ( 4 )

/**
 * @brief The length of each topic filter.
 *
 * For convenience, all topic filters are the same length.
 */
#define TOPIC_FILTER_LENGTH                      ( ( uint16_t ) ( sizeof( IOT_DEMO_MQTT_TOPIC_PREFIX "/topic/1" ) - 1 ) )

/**
 * @brief Format string of the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_FORMAT                   "Hello world %d!"

/**
 * @brief Size of the buffer that holds the PUBLISH messages in this demo.
 */
#define PUBLISH_PAYLOAD_BUFFER_LENGTH            ( sizeof( PUBLISH_PAYLOAD_FORMAT ) + 2 )

/**
 * @brief The maximum number of times each PUBLISH in this demo will be retried.
 */
#define PUBLISH_RETRY_LIMIT                      ( 10 )

/**
 * @brief A PUBLISH message is retried if no response is received within this
 * time.
 */
#define PUBLISH_RETRY_MS                         ( 1000 )

/**
 * @brief The topic name on which acknowledgement messages for incoming publishes
 * should be published.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME               IOT_DEMO_MQTT_TOPIC_PREFIX "/acknowledgements"

/**
 * @brief The length of #ACKNOWLEDGEMENT_TOPIC_NAME.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME_LENGTH        ( ( uint16_t ) ( sizeof( ACKNOWLEDGEMENT_TOPIC_NAME ) - 1 ) )

/**
 * @brief Format string of PUBLISH acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_FORMAT           "Client has received PUBLISH %.*s from server."

/**
 * @brief Size of the buffers that hold acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH    ( sizeof( ACKNOWLEDGEMENT_MESSAGE_FORMAT ) + 2 )

/*-----------------------------------------------------------*/

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
/* Declaration of shadow demo function. */
int RunShadowDemoShared( const char * pIdentifier,
                         const IotMqttConnection_t mqttConnection );

/* Declaration of mqtt demo function. */
int RunMqttDemoShared( const IotMqttConnection_t mqttConnection );

/* Declaration of demo function. */
int RunMqttShadowDemo( bool awsIotMqttMode,
                       const char * pIdentifier,
                       void * pNetworkServerInfo,
                       void * pNetworkCredentialInfo,
                       const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/

/**
 * @brief Called by the MQTT library when an operation completes.
 *
 * The demo uses this callback to determine the result of PUBLISH operations.
 * @param[in] param1 The number of the PUBLISH that completed, passed as an intptr_t.
 * @param[in] pOperation Information about the completed operation passed by the
 * MQTT library.
 */
static void _clientPublishedCallback(void * param1,
                                           IotMqttCallbackParam_t * const pOperation )
{
    intptr_t publishCount = ( intptr_t ) param1;

    /* Silence warnings about unused variables. publishCount will not be used if
     * logging is disabled. */
    ( void ) publishCount;

    /* Print the status of the completed operation. A PUBLISH operation is
     * successful when transmitted over the network. */
    if( pOperation->u.operation.result == IOT_MQTT_SUCCESS )
    {
        IotLogInfo( "MQTT %s %d successfully sent.",
                    IotMqtt_OperationType( pOperation->u.operation.type ),
                    ( int ) publishCount );
    }
    else
    {
        IotLogError( "MQTT %s %d could not be sent. Error %s.",
                     IotMqtt_OperationType( pOperation->u.operation.type ),
                     ( int ) publishCount,
                     IotMqtt_strerror( pOperation->u.operation.result ) );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Called by the MQTT library when an incoming PUBLISH message is received.
 *
 * The demo uses this callback to handle incoming PUBLISH messages. This callback
 * prints the contents of an incoming message and publishes an acknowledgement
 * to the MQTT server.
 * @param[in] param1 Counts the total number of received PUBLISH messages. This
 * callback will increment this counter.
 * @param[in] pPublish Information about the incoming PUBLISH message passed by
 * the MQTT library.
 */
static void _serverPublishedCallback( void * param1,
                                      IotMqttCallbackParam_t * const pPublish )
{
    int acknowledgementLength = 0;
    size_t messageNumberIndex = 0, messageNumberLength = 1;
    IotSemaphore_t * pPublishesReceived = ( IotSemaphore_t * ) param1;
    const char * pPayload = pPublish->u.message.info.pPayload;
    char pAcknowledgementMessage[ ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH ] = { 0 };
    IotMqttPublishInfo_t acknowledgementInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;

    /* Print information about the incoming PUBLISH message. */
    IotLogInfo( "Incoming PUBLISH received:\r\n"
                "Subscription topic filter: %.*s\r\n"
                "Publish topic name: %.*s\r\n"
                "Publish retain flag: %d\r\n"
                "Publish QoS: %d\r\n"
                "Publish payload: %.*s",
                pPublish->u.message.topicFilterLength,
                pPublish->u.message.pTopicFilter,
                pPublish->u.message.info.topicNameLength,
                pPublish->u.message.info.pTopicName,
                pPublish->u.message.info.retain,
                pPublish->u.message.info.qos,
                pPublish->u.message.info.payloadLength,
                pPayload );

    /* Find the message number inside of the PUBLISH message. */
    for( messageNumberIndex = 0; messageNumberIndex < pPublish->u.message.info.payloadLength; messageNumberIndex++ )
    {
        /* The payload that was published contained ASCII characters, so find
         * beginning of the message number by checking for ASCII digits. */
        if( ( pPayload[ messageNumberIndex ] >= '0' ) &&
            ( pPayload[ messageNumberIndex ] <= '9' ) )
        {
            break;
        }
    }

    /* Check that a message number was found within the PUBLISH payload. */
    if( messageNumberIndex < pPublish->u.message.info.payloadLength )
    {
        /* Compute the length of the message number. */
        while( ( messageNumberIndex + messageNumberLength < pPublish->u.message.info.payloadLength ) &&
               ( *( pPayload + messageNumberIndex + messageNumberLength ) >= '0' ) &&
               ( *( pPayload + messageNumberIndex + messageNumberLength ) <= '9' ) )
        {
            messageNumberLength++;
        }

        /* Generate an acknowledgement message. */
        acknowledgementLength = snprintf( pAcknowledgementMessage,
                                          ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH,
                                          ACKNOWLEDGEMENT_MESSAGE_FORMAT,
                                          ( int ) messageNumberLength,
                                          pPayload + messageNumberIndex );

        /* Check for errors from snprintf. */
        if( acknowledgementLength < 0 )
        {
            IotLogWarn( "Failed to generate acknowledgement message for PUBLISH %.*s.",
                        ( int ) messageNumberLength,
                        pPayload + messageNumberIndex );
        }
        else
        {
            /* Set the members of the publish info for the acknowledgement message. */
            acknowledgementInfo.qos = IOT_MQTT_QOS_1;
            acknowledgementInfo.pTopicName = ACKNOWLEDGEMENT_TOPIC_NAME;
            acknowledgementInfo.topicNameLength = ACKNOWLEDGEMENT_TOPIC_NAME_LENGTH;
            acknowledgementInfo.pPayload = pAcknowledgementMessage;
            acknowledgementInfo.payloadLength = ( size_t ) acknowledgementLength;
            acknowledgementInfo.retryMs = PUBLISH_RETRY_MS;
            acknowledgementInfo.retryLimit = PUBLISH_RETRY_LIMIT;

            /* Send the acknowledgement for the received message. This demo program
             * will not be notified on the status of the acknowledgement because
             * neither a callback nor IOT_MQTT_FLAG_WAITABLE is set. However,
             * the MQTT library will still guarantee at-least-once delivery (subject
             * to the retransmission strategy) because the acknowledgement message is
             * sent at QoS 1. */
            if( IotMqtt_Publish( pPublish->mqttConnection,
                                 &acknowledgementInfo,
                                 0,
                                 NULL,
                                 NULL ) == IOT_MQTT_STATUS_PENDING )
            {
                IotLogInfo( "Acknowledgment message for PUBLISH %.*s will be sent.",
                            ( int ) messageNumberLength,
                            pPayload + messageNumberIndex );
            }
            else
            {
                IotLogWarn( "Acknowledgment message for PUBLISH %.*s will NOT be sent.",
                            ( int ) messageNumberLength,
                            pPayload + messageNumberIndex );
            }
        }
    }

    /* Increment the number of PUBLISH messages received. */
    IotSemaphore_Post( pPublishesReceived );
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
 * @brief Add or remove subscriptions by either subscribing or unsubscribing.
 *
 * @param[in] mqttConnection The MQTT connection to use for subscriptions.
 * @param[in] operation Either #IOT_MQTT_SUBSCRIBE or #IOT_MQTT_UNSUBSCRIBE.
 * @param[in] pTopicFilters Array of topic filters for subscriptions.
 * @param[in] pCallbackParameter The parameter to pass to the subscription
 * callback.
 *
 * @return `EXIT_SUCCESS` if the subscription operation succeeded; `EXIT_FAILURE`
 * otherwise.
 */
static int _modifySubscriptions( const IotMqttConnection_t mqttConnection,
                                 IotMqttOperationType_t operation,
                                 const char ** pTopicFilters,
                                 void * pCallbackParameter )
{
    int status = EXIT_SUCCESS;
    int32_t i = 0;
    IotMqttError_t subscriptionStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttSubscription_t pSubscriptions[ TOPIC_FILTER_COUNT ] = { IOT_MQTT_SUBSCRIPTION_INITIALIZER };

    /* Set the members of the subscription list. */
    for( i = 0; i < TOPIC_FILTER_COUNT; i++ )
    {
        pSubscriptions[ i ].qos = IOT_MQTT_QOS_1;
        pSubscriptions[ i ].pTopicFilter = pTopicFilters[ i ];
        pSubscriptions[ i ].topicFilterLength = TOPIC_FILTER_LENGTH;
        pSubscriptions[ i ].callback.pCallbackContext = pCallbackParameter;
        pSubscriptions[ i ].callback.function = _serverPublishedCallback;
    }

    /* Modify subscriptions by either subscribing or unsubscribing. */
    if( operation == IOT_MQTT_SUBSCRIBE )
    {
        subscriptionStatus = IotMqtt_TimedSubscribe( mqttConnection,
                                                     pSubscriptions,
                                                     TOPIC_FILTER_COUNT,
                                                     0,
                                                     TIMEOUT_MS );

        /* Check the status of SUBSCRIBE. */
        switch( subscriptionStatus )
        {
            case IOT_MQTT_SUCCESS:
                IotLogInfo( "All demo topic filter subscriptions accepted." );
                break;

            case IOT_MQTT_SERVER_REFUSED:

                /* Check which subscriptions were rejected before exiting the demo. */
                for( i = 0; i < TOPIC_FILTER_COUNT; i++ )
                {
                    if( IotMqtt_IsSubscribed( mqttConnection,
                                              pSubscriptions[ i ].pTopicFilter,
                                              pSubscriptions[ i ].topicFilterLength,
                                              NULL ) == true )
                    {
                        IotLogInfo( "Topic filter %.*s was accepted.",
                                    pSubscriptions[ i ].topicFilterLength,
                                    pSubscriptions[ i ].pTopicFilter );
                    }
                    else
                    {
                        IotLogError( "Topic filter %.*s was rejected.",
                                     pSubscriptions[ i ].topicFilterLength,
                                     pSubscriptions[ i ].pTopicFilter );
                    }
                }

                status = EXIT_FAILURE;
                break;

            default:

                status = EXIT_FAILURE;
                break;
        }
    }
    else if( operation == IOT_MQTT_UNSUBSCRIBE )
    {
        subscriptionStatus = IotMqtt_TimedUnsubscribe( mqttConnection,
                                                       pSubscriptions,
                                                       TOPIC_FILTER_COUNT,
                                                       0,
                                                       TIMEOUT_MS );

        /* Check the status of UNSUBSCRIBE. */
        if( subscriptionStatus != IOT_MQTT_SUCCESS )
        {
            status = EXIT_FAILURE;
        }
    }
    else
    {
        /* Only SUBSCRIBE and UNSUBSCRIBE are valid for modifying subscriptions. */
        IotLogError( "MQTT operation %s is not valid for modifying subscriptions.",
                     IotMqtt_OperationType( operation ) );

        status = EXIT_FAILURE;
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Transmit all messages and wait for them to be received on topic filters.
 *
 * @param[in] mqttConnection The MQTT connection to use for publishing.
 * @param[in] pTopicNames Array of topic names for publishing. These were previously
 * subscribed to as topic filters.
 * @param[in] pPublishReceivedCounter Counts the number of messages received on
 * topic filters.
 *
 * @return `EXIT_SUCCESS` if all messages are published and received; `EXIT_FAILURE`
 * otherwise.
 */
static int _publishAllMessages( const IotMqttConnection_t mqttConnection,
                                const char ** pTopicNames,
                                IotSemaphore_t * pPublishReceivedCounter )
{
    int status = EXIT_SUCCESS;
    intptr_t publishCount = 0, i = 0;
    IotMqttError_t publishStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttPublishInfo_t publishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    IotMqttCallbackInfo_t publishComplete = IOT_MQTT_CALLBACK_INFO_INITIALIZER;
    char pPublishPayload[ PUBLISH_PAYLOAD_BUFFER_LENGTH ] = { 0 };

    /* The MQTT library should invoke this callback when a PUBLISH message
     * is successfully transmitted. */
    publishComplete.function = _clientPublishedCallback;

    /* Set the common members of the publish info. */
    publishInfo.qos = IOT_MQTT_QOS_1;
    publishInfo.topicNameLength = TOPIC_FILTER_LENGTH;
    publishInfo.pPayload = pPublishPayload;
    publishInfo.retryMs = PUBLISH_RETRY_MS;
    publishInfo.retryLimit = PUBLISH_RETRY_LIMIT;

    /* Loop to PUBLISH all messages of this demo. */
    for( publishCount = 0;
         publishCount < IOT_DEMO_MQTT_PUBLISH_BURST_SIZE * IOT_DEMO_MQTT_PUBLISH_BURST_COUNT;
         publishCount++ )
    {
        /* Announce which burst of messages is being published. */
        if( publishCount % IOT_DEMO_MQTT_PUBLISH_BURST_SIZE == 0 )
        {
            IotLogInfo( "Publishing messages %d to %d.",
                        publishCount,
                        publishCount + IOT_DEMO_MQTT_PUBLISH_BURST_SIZE - 1 );
        }

        /* Pass the PUBLISH number to the operation complete callback. */
        publishComplete.pCallbackContext = ( void * ) publishCount;

        /* Choose a topic name (round-robin through the array of topic names). */
        publishInfo.pTopicName = pTopicNames[ publishCount % TOPIC_FILTER_COUNT ];

        /* Generate the payload for the PUBLISH. */
        status = snprintf( pPublishPayload,
                           PUBLISH_PAYLOAD_BUFFER_LENGTH,
                           PUBLISH_PAYLOAD_FORMAT,
                           ( int ) publishCount );

        /* Check for errors from snprintf. */
        if( status < 0 )
        {
            IotLogError( "Failed to generate MQTT PUBLISH payload for PUBLISH %d.",
                         ( int ) publishCount );
            status = EXIT_FAILURE;

            break;
        }
        else
        {
            publishInfo.payloadLength = ( size_t ) status;
            status = EXIT_SUCCESS;
        }

        /* PUBLISH a message. This is an asynchronous function that notifies of
         * completion through a callback. */
        publishStatus = IotMqtt_Publish( mqttConnection,
                                         &publishInfo,
                                         0,
                                         &publishComplete,
                                         NULL );

        if( publishStatus != IOT_MQTT_STATUS_PENDING )
        {
            IotLogError( "MQTT PUBLISH %d returned error %s.",
                         ( int ) publishCount,
                         IotMqtt_strerror( publishStatus ) );
            status = EXIT_FAILURE;

            break;
        }

        /* If a complete burst of messages has been published, wait for an equal
         * number of messages to be received. Note that messages may be received
         * out-of-order, especially if a message was lost and had to be retried. */
        if( ( publishCount % IOT_DEMO_MQTT_PUBLISH_BURST_SIZE ) ==
            ( IOT_DEMO_MQTT_PUBLISH_BURST_SIZE - 1 ) )
        {
            IotLogInfo( "Waiting for %d publishes to be received.",
                        IOT_DEMO_MQTT_PUBLISH_BURST_SIZE );

            for( i = 0; i < IOT_DEMO_MQTT_PUBLISH_BURST_SIZE; i++ )
            {
                if( IotSemaphore_TimedWait( pPublishReceivedCounter,
                                            TIMEOUT_MS ) == false )
                {
                    IotLogError( "Timed out waiting for incoming PUBLISH messages." );
                    status = EXIT_FAILURE;
                    break;
                }
            }

            IotLogInfo( "%d publishes received.",
                        i );
        }

        /* Stop publishing if there was an error. */
        if( status == EXIT_FAILURE )
        {
            break;
        }
    }

    return status;
}

/**
 * @brief The function that runs the MQTT demo.
 *
 * @param[in] mqttConnection: An already successfully established MQTT Connection Handle. Intended to be shared with
 *                            other functions.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunMqttDemoShared( const IotMqttConnection_t mqttConnection )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Counts the number of incoming PUBLISHES received (and allows the demo
     * application to wait on incoming PUBLISH messages). */
    IotSemaphore_t publishesReceived;

    /* Topics used as both topic filters and topic names in this demo. */
    const char * pTopics[ TOPIC_FILTER_COUNT ] =
            {
                    IOT_DEMO_MQTT_TOPIC_PREFIX "/topic/1",
                    IOT_DEMO_MQTT_TOPIC_PREFIX "/topic/2",
                    IOT_DEMO_MQTT_TOPIC_PREFIX "/topic/3",
                    IOT_DEMO_MQTT_TOPIC_PREFIX "/topic/4",
            };

    if( status == EXIT_SUCCESS )
    {
        /* Add the topic filter subscriptions used in this demo. */
        status = _modifySubscriptions( mqttConnection,
                                       IOT_MQTT_SUBSCRIBE,
                                       pTopics,
                                       &publishesReceived );
    }

    if( status == EXIT_SUCCESS )
    {
        /* Create the semaphore to count incoming PUBLISH messages. */
        if( IotSemaphore_Create( &publishesReceived,
                                 0,
                                 IOT_DEMO_MQTT_PUBLISH_BURST_SIZE ) == true )
        {
            /* PUBLISH (and wait) for all messages. */
            status = _publishAllMessages( mqttConnection,
                                          pTopics,
                                          &publishesReceived );

            /* Destroy the incoming PUBLISH counter. */
            IotSemaphore_Destroy( &publishesReceived );
        }
        else
        {
            /* Failed to create incoming PUBLISH counter. */
            status = EXIT_FAILURE;
        }
    }

    if( status == EXIT_SUCCESS )
    {
        /* Remove the topic subscription filters used in this demo. */
        status = _modifySubscriptions( mqttConnection,
                                       IOT_MQTT_UNSUBSCRIBE,
                                       pTopics,
                                       NULL );
    }

    return status;
}

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


/**
 * @brief Establish a new connection to the MQTT server for the Mqtt+Shadow demo. Note that since this connection is
 *        shared with Shadow functionality, which is specific to Iot Core, we are obliged to have IotMqttMode=true,
 *        which configures keep-alive behaviour that is specific to IoT-Core
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
    IotMqttPublishInfo_t willInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;

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
        connectInfo.pWillInfo = &willInfo;

        /* Set the members of the Last Will and Testament (LWT) message info. The
         * MQTT server will publish the LWT message if this client disconnects
         * unexpectedly. */
        willInfo.pTopicName = WILL_TOPIC_NAME;
        willInfo.topicNameLength = WILL_TOPIC_NAME_LENGTH;
        willInfo.pPayload = WILL_MESSAGE;
        willInfo.payloadLength = WILL_MESSAGE_LENGTH;

        /* AWS IoT recommends the use of the Thing Name as the MQTT client ID. */
        connectInfo.pClientIdentifier = pIdentifier;
        connectInfo.clientIdentifierLength = ( uint16_t ) strlen( pIdentifier );

        IotLogInfo( "Shadow Thing Name and MQTT Client identifer are %.*s (length %hu).",
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
            IotLogError( "IotMqtt_Connect returned error %s.",
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
                                const IotMqttConnection_t mqttConnection,
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

/**
 * @brief The function that runs the Shadow demo.
 *
 * @param[in] mqttConnection: An already successfully established MQTT Connection Handle. Intended to be shared with
 *                            other functions.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunShadowDemoShared( const char * pIdentifier,
                   const IotMqttConnection_t mqttConnection )
{
    /* Return value of this function and the exit status of this program. */
    int status = 0;

    /* Length of Shadow Thing Name. */
    size_t thingNameLength = 0;

    /* Allows the Shadow update function to wait for the delta callback to complete
     * a state change before continuing. */
    IotSemaphore_t deltaSemaphore;

    /* Flags for tracking which cleanup functions must be called. */
    bool deltaSemaphoreCreated = false;

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

    if( status == EXIT_SUCCESS )
    {
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

    /* Destroy the delta semaphore if it was created. */
    if( deltaSemaphoreCreated == true )
    {
        IotSemaphore_Destroy( &deltaSemaphore );
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief The function that runs the MQTT demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunMqttShadowDemo( bool awsIotMqttMode,
                 const char * pIdentifier,
                 void * pNetworkServerInfo,
                 void * pNetworkCredentialInfo,
                 const IotNetworkInterface_t * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    /* Handle of the MQTT connection to be shared by MQTT and Shadow demos */
    IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Flags for tracking which cleanup functions must be called. */
    bool librariesInitialized = false;

    /* Initialize the libraries required for mqtt and shadow demos. */
    status = _initializeDemo();

    if ( status == EXIT_SUCCESS )
    {
        librariesInitialized = true;

        /* Establish a new MQTT connection. */
        status = _establishMqttConnection(pIdentifier,
                                          pNetworkServerInfo,
                                          pNetworkCredentialInfo,
                                          pNetworkInterface,
                                          &mqttConnection);
    }


    /* Run the demo if the libraries and mqtt connection were started successfully */
    if ( status == EXIT_SUCCESS )
    {
        int statusMqtt = RunMqttDemoShared( mqttConnection );
        int statusShadow = RunShadowDemoShared( pIdentifier, mqttConnection );

        if ( statusMqtt == EXIT_SUCCESS && statusShadow == EXIT_SUCCESS)
        {
            status = EXIT_SUCCESS;
        }
        else
        {
            status = EXIT_FAILURE;
        }

        IotMqtt_Disconnect( mqttConnection, 0 );
    }

    /* Clean up libraries if they were initialized. */
    if( librariesInitialized )
    {
        _cleanupDemo();
    }



    return status;
}

/*-----------------------------------------------------------*/
