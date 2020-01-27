
/*
 * AppXDK_MQTT.c
 *
 *  Created on: 31 Jul 2019
 *      Author: rjgu
 *
 *      original module adapted for this app.
 */
/**
 * @defgroup AppXDK_MQTT AppXDK_MQTT
 * @{
 *
 * @brief Implements the MQTT interface to the ServalPal module.
 * Adapted from the original XDK module: XDK_MQTT.h and MQTT.c.
 * @details Added single use / single call protection using semaphores. Added internal state management.
 *
 * @author $(SOLACE_APP_AUTHOR)
 *
 * @date $(SOLACE_APP_DATE)
 *
 * @file
 *
 */

#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID /**< undefine any previous module id */
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_XDK_MQTT

#include "AppXDK_MQTT.h"
#include "AppMisc.h"

#include <stdio.h>

#include "MbedTLSAdapter.h"
#include "HTTPRestClientSecurity.h"
#include "BCDS_NetworkConfig.h"
#include <Serval_Mqtt.h>
#include "Serval_Msg.h"
#include "Serval_Http.h"
#include "Serval_HttpClient.h"
#include "Serval_Types.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "BCDS_BSP_Board.h"


/**
 * @brief Enum to represent the current state of the module.
 */
typedef enum {
	AppXDK_MQTT_State_Ready, /**< ready state */
	AppXDK_MQTT_State_Connecting, /**< currently connecting */
	AppXDK_MQTT_State_Publishing, /**< currently publishing */
	AppXDK_MQTT_State_Subscribing, /**< currently subscribing */
	AppXDK_MQTT_State_Unsubscribing, /**< current unsubscribing */
} AppXDK_MQTT_State_T;

#define APP_XDK_MQTT_MAX_UNSUBSCRIBE_COUNT          10UL /**<  the max number of topics to unsubscribe from in one call */

#define APP_XDK_MQTT_URL_FORMAT_NON_SECURE          "mqtt://%s:%d" /**<  the non-secure serval stack expected MQTT URL format */

#define APP_XDK_MQTT_URL_FORMAT_SECURE              "mqtts://%s:%d" /**<  the secure serval stack expected MQTT URL format */



static SemaphoreHandle_t appXDK_MQTT_ExternalInterface_SemaphoreHandle; /**< external interface semaphore, allows only 1 active external call at a time */
#define APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_CONNECT_SEMAPHORE_WAIT_IN_MS		UINT32_C(60000) /**< wait for module to become free for connect call */
#define APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_SUBSCRIBE_SEMAPHORE_WAIT_IN_MS		UINT32_C(60000) /**< wait for module to become free for subscribe call */
#define APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_UNSUBSCRIBE_SEMAPHORE_WAIT_IN_MS	UINT32_C(60000) /**< wait for module to become free for unsubscribe call */
//qos1 can take longer than qos0, and for qos0 (mostly telemetry messages) we don't care so much about losing a message
#define APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_0_SEMAPHORE_WAIT_IN_MS		UINT32_C(100)	/**< wait for module to become free for publish call for qos0 messages */
#define APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_1_SEMAPHORE_WAIT_IN_MS		APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS + UINT32_C(100)	/**< wait for module to become free for publish call for qos1 messages */

static AppXDK_MQTT_State_T appXDK_MQTT_State = AppXDK_MQTT_State_Ready; /**< internal state with default */

static SemaphoreHandle_t appXDK_MQTT_SubscribeSemaphoreHandle; /**< internal subscribe semaphore */
static SemaphoreHandle_t appXDK_MQTT_UnsubscribeSemaphoreHandle; /**< internal unsubscribe semaphore */
static SemaphoreHandle_t appXDK_MQTT_PublishSemaphoreHandle; /**< internal publish semaphore */
static SemaphoreHandle_t appXDK_MQTT_ConnectSemaphoreHandle; /**< internal connect semaphore */

static AppXDK_MQTT_Setup_T appXDK_MQTT_SetupInfo; /**< mqtt setup info */

static MqttSession_T appXDK_MQTT_ServalSession; /**< serval session info */

static bool appXDK_MQTT_AppInitiatedInteraction = false; /**< flag to indicate that interaction was initiated by the application (externally) */
static bool appXDK_MQTT_ConnectionStatus = false; /**< flag to indicate connection status between caller and event handler */
static bool appXDK_MQTT_SubscriptionStatus = false; /**< flag to indicate subscription status between caller and event handler */
static bool appXDK_MQTT_UnsubscribeStatus = false; /**< flag to indicate unsubscribe status between caller and event handler */
static bool appXDK_MQTT_PublishStatus = false; /**< flag to indicate publish status between caller and event handler */
static Retcode_T appXDK_MQTT_EventHandler_Retcode = RETCODE_OK; /**< event handler retcode to consume in caller */
static MqttEvent_t appXDK_MQTT_EventHandler_ServalEvent = -1; /**< the serval event from event handler call */
static MqttEvent_t appXDK_MQTT_EventHandler_PriorServalEvent = -1; /**< the prior serval event, from pervious event handler call */

/**
 * @brief Translates internal state to a retcode for caller.
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNDEFINED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_STATE_ERROR)
 */
static Retcode_T appXDK_MQTT_GetModuleBusyRetcode(void) {
	Retcode_T retcode = RETCODE_OK;

	// note: between checking and calling this function the state may have changed
	// need to introduce a semaphore to protect appXDK_MQTT_State

	switch(appXDK_MQTT_State) {
	case AppXDK_MQTT_State_Connecting:
		retcode = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING);
		break;
	case AppXDK_MQTT_State_Publishing:
		retcode = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING);
		break;
	case AppXDK_MQTT_State_Subscribing:
		retcode = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING);
		break;
	case AppXDK_MQTT_State_Unsubscribing:
		retcode = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING);
		break;
	case AppXDK_MQTT_State_Ready:
		retcode = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNDEFINED);
		break;
	default:
		retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_STATE_ERROR);
		Retcode_RaiseError(retcode);
		break;
	}
	return retcode;
}
/**
 * @brief Event handler for incoming MQTT data.
 * @details Copies the incoming structure to #AppXDK_MQTT_IncomingDataCallbackParam_T and calls the initialized #AppXDK_MQTT_IncomingDataCallback_Func_T in @ref appXDK_MQTT_SetupInfo .incomingDataCallBack_Func.
 * @note Make sure to delete the incoming data after processing it using @ref AppXDK_MQTT_FreeSubscribeCallbackParams().
 * @param[in] incomingData: the incoming data structure
 */
static void appXDK_MQTT_HandleEventIncomingDataCallback(MqttPublishData_T incomingData) {

	char * topic = malloc(incomingData.topic.length);
	memcpy(topic, incomingData.topic.start, incomingData.topic.length);

	char * payload = malloc(incomingData.length);
	memcpy(payload, incomingData.payload, incomingData.length);

	AppXDK_MQTT_IncomingDataCallbackParam_T * params = malloc(sizeof(AppXDK_MQTT_IncomingDataCallbackParam_T));
	params->topic = topic;
	params->topicLength = incomingData.topic.length;
	params->payload = payload;
	params->payloadLength = incomingData.length;

	appXDK_MQTT_SetupInfo.incomingDataCallBack_Func(params);
}

/**
 * @brief Frees the subscription parameters received in the subscribe callback as specified in the subscription.
 * @param[in] params: the subscription message
 */
void AppXDK_MQTT_FreeSubscribeCallbackParams(AppXDK_MQTT_IncomingDataCallbackParam_T * params) {

	if(params == NULL) return;
	free(params->topic);
	free(params->payload);
	free(params);

}

#ifdef DEBUG_APP_XDK_MQTT
/**
 * @brief Returns the event string from the event number. Useful for debugging.
 * @param[in] servalEvent: the event received in the callback
 * @return char *: pointer to a static event string
 */
static char * appXDK_MQTT_GetEventStr(MqttEvent_t servalEvent) {

	char * eventStr = NULL;
	switch (servalEvent) {
		case MQTT_CONNECTION_ESTABLISHED: 				eventStr = "MQTT_CONNECTION_ESTABLISHED"; break;
	    case MQTT_CONNECTION_ERROR: 					eventStr = "MQTT_CONNECTION_ERROR"; break;
	    case MQTT_CONNECT_SEND_FAILED: 					eventStr = "MQTT_CONNECT_SEND_FAILED"; break;
	    case MQTT_CONNECT_TIMEOUT:						eventStr = "MQTT_CONNECT_TIMEOUT"; break;
	    case MQTT_CONNECTION_CLOSED:					eventStr = "MQTT_CONNECTION_CLOSED"; break;
	    case MQTT_SUBSCRIPTION_ACKNOWLEDGED:			eventStr = "MQTT_SUBSCRIPTION_ACKNOWLEDGED"; break;
		case MQTT_SUBSCRIBE_SEND_FAILED:				eventStr = "MQTT_SUBSCRIBE_SEND_FAILED"; break;
		case MQTT_SUBSCRIBE_TIMEOUT:					eventStr = "MQTT_SUBSCRIBE_TIMEOUT"; break;
	    case MQTT_SUBSCRIPTION_REMOVED: 				eventStr = "MQTT_SUBSCRIPTION_REMOVED"; break;
	    case MQTT_INCOMING_PUBLISH:						eventStr = "MQTT_INCOMING_PUBLISH"; break;
	    case MQTT_PUBLISHED_DATA:						eventStr = "MQTT_PUBLISHED_DATA"; break;
	    case MQTT_PUBLISH_SEND_FAILED:					eventStr = "MQTT_PUBLISH_SEND_FAILED"; break;
	    case MQTT_PUBLISH_SEND_ACK_FAILED:				eventStr = "MQTT_PUBLISH_SEND_ACK_FAILED"; break;
		case MQTT_PUBLISH_TIMEOUT:						eventStr = "MQTT_PUBLISH_TIMEOUT"; break;
		case MQTT_PING_RESPONSE_RECEIVED:				eventStr = "MQTT_PING_RESPONSE_RECEIVED"; break;
		case MQTT_PING_SEND_FAILED:						eventStr = "MQTT_PING_SEND_FAILED"; break;
		case MQTT_SERVER_DID_NOT_RELEASE:				eventStr = "MQTT_SERVER_DID_NOT_RELEASE"; break;
		case MQTT_DISCONNECT_SEND_FAILED:				eventStr = "MQTT_DISCONNECT_SEND_FAILED"; break;
		case MQTT_UNSUBSCRIBE_SEND_FAILED:				eventStr = "MQTT_UNSUBSCRIBE_SEND_FAILED"; break;
		case MQTT_UNSUBSCRIBE_TIMEOUT:					eventStr = "MQTT_UNSUBSCRIBE_TIMEOUT"; break;
		default:										eventStr = "UNKNOWN_EVENT"; break;
	}
	return eventStr;
}
/**
 * @brief Prints debug event info on the console.
 * @param[in] counter: an increasing counter for every event received
 * @param[in] event: the last event received
 * @param[in] priorEvent: the event received prior to last event
 */
static void appXDK_MQTT_PrintEvents(uint32_t counter, MqttEvent_t event, MqttEvent_t priorEvent) {
	if(event == MQTT_PUBLISHED_DATA) {
		#ifdef DEBUG_APP_XDK_MQTT_EVERY_PUBLISHED_DATA_CALLBACK
		printf("[INFO] - appXDK_MQTT_EventHandler: counter: %lu, event: %i = %s (%i=%s)\r\n", counter, event, appXDK_MQTT_GetEventStr(event), priorEvent, appXDK_MQTT_GetEventStr(priorEvent));
		#endif
	} else {
		printf("[INFO] - appXDK_MQTT_EventHandler: counter: %lu, event: %i:%s (prior: %i:%s)\r\n", counter, event, appXDK_MQTT_GetEventStr(event), priorEvent, appXDK_MQTT_GetEventStr(priorEvent));
	}
}
#endif

/**
 * @brief Callback function used by the stack to communicate events to the application.
 * Each event will bring with it specialized data that will contain more information.
 *
 * @details Module ensures there is only one callback running at any time.
 * The initiator of the interaction - @ref AppXDK_MQTT_ConnectToBroker(), @ref AppXDK_MQTT_SubsribeToTopic(), @ref AppXDK_MQTT_UnsubsribeFromTopics(), @ref AppXDK_MQTT_PublishToTopic()
 * will block on (take) a semaphore. This callback event handler will release the semaphore so the initiator can continue synchronously.
 * Implements a typical async -> sync conversion using semaphores.
 *
 * @details Sets the #appXDK_MQTT_EventHandler_Retcode variable for the initiator of the interaction to read:
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR)
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR)
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR)
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR)
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_UNHANDLED_MQTT_EVENT)
 * @details - #appXDK_MQTT_EventHandler_Retcode: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_UNKNOWN_CALLBACK_EVENT_RECEIVED)
 *
 * @details Sets the following status variables for the initiator of the interaction to read:
 * @details - #appXDK_MQTT_ConnectionStatus: true for ok, false for failed
 * @details - #appXDK_MQTT_SubscriptionStatus: true for ok, false for failed
 * @details - #appXDK_MQTT_UnsubscribeStatus: true for ok, false for failed
 * @details - #appXDK_MQTT_PublishStatus: true for ok, false for failed
 *
 * @details Manages / sets the following static variables:
 * @details - #appXDK_MQTT_EventHandler_ServalEvent
 * @details - #appXDK_MQTT_EventHandler_PriorServalEvent
 *
 * @param[in] servalSession: unused
 * @param[in] servalEvent: event type
 * @param[in] servalEventData: the data associated for this event type, see MqttEventData_u
 *
 * @return retcode_t: always RC_OK
 *
 * @code
 * union MqttEventData_u {
 *  MqttConnectionEstablishedEvent_T connect;
 *  MqttSubscriptionData_T subscription;
 *  MqttPublishData_T publish;
 *  MqttSession_T session;
 *  retcode_t status;
 * } MqttEventData_t;
 *
 * @endcode
 *
 * @details Events communicated to the application using the onMqttEvent callback in the session.
 * Each event carries a particular data type which describes further the event.
 *
 * @details The mapping of the MqttEvent_t type field to MqttEventData_t fields is:
 *
 * @details - MQTT_CONNECTION_ESTABLISHED    -> MqttConnectionEstablishedEvent_T,
 * @details - MQTT_CONNECTION_ERROR          -> MqttConnectionEstablishedEvent_T,
 * @details - MQTT_CONNECT_SEND_FAILED       -> NULL,
 * @details - MQTT_CONNECT_TIMEOUT           -> NULL,
 * @details - MQTT_CONNECTION_CLOSED         -> NULL,
 * @details - MQTT_DISCONNECT_SEND_FAILED    -> NULL,
 * @details - MQTT_PING_SEND_FAILED          -> NULL,
 * @details - MQTT_PING_RESPONSE_RECEIVED    -> NULL,
 * @details - MQTT_SUBSCRIPTION_ACKNOWLEDGED -> MqttSubscriptionData_T,
 * @details - MQTT_SUBSCRIBE_SEND_FAILED     -> MqttSubscriptionData_T,
 * @details - MQTT_SUBSCRIBE_TIMEOUT         -> MqttSubscriptionData_T,
 * @details - MQTT_SUBSCRIPTION_REMOVED      -> MqttSubscriptionData_T,
 * @details - MQTT_UNSUBSCRIBE_TIMEOUT       -> MqttSubscriptionData_T,
 * @details - MQTT_UNSUBSCRIBE_SEND_FAILED   -> MqttSubscriptionData_T,
 * @details - MQTT_INCOMING_PUBLISH          -> MqttPublishData_T,
 * @details - MQTT_PUBLISHED_DATA            -> MqttPublishData_T,
 * @details - MQTT_PUBLISH_TIMEOUT           -> MqttPublishData_T,
 * @details - MQTT_PUBLISH_SEND_FAILED       -> MqttPublishData_T,
 * @details - MQTT_PUBLISH_SEND_ACK_FAILED   -> NULL,
 * @details - MQTT_SERVER_DID_NOT_RELEASE    -> NULL,
 *
 */
static retcode_t appXDK_MQTT_EventHandler(MqttSession_T * servalSession, MqttEvent_t servalEvent, const MqttEventData_t * servalEventData) {

	BCDS_UNUSED(servalSession);

	appXDK_MQTT_EventHandler_Retcode = RETCODE_OK;

    appXDK_MQTT_EventHandler_ServalEvent = servalEvent;

#ifdef DEBUG_APP_XDK_MQTT
    static uint32_t eventHandler_Counter = 0;
    eventHandler_Counter++;
    appXDK_MQTT_PrintEvents(eventHandler_Counter, appXDK_MQTT_EventHandler_ServalEvent, appXDK_MQTT_EventHandler_PriorServalEvent);
#endif

    switch (servalEvent) {

    /*
     * connection handling
     */
    case MQTT_CONNECTION_ESTABLISHED:
    	appXDK_MQTT_ConnectionStatus = true;
        if (pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
        break;
    case MQTT_CONNECTION_ERROR:
    	/* test:
    	 * - set the enabled flag to false in the session
    	 */
    	appXDK_MQTT_ConnectionStatus = false;
        if (pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
        break;
    case MQTT_CONNECT_SEND_FAILED:
    	/* test:
    	 * - not tested / observed
    	 */
    	appXDK_MQTT_ConnectionStatus = false;
        if (pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
        break;
    case MQTT_CONNECT_TIMEOUT:
    	/*
    	 * note:
    	 * - could get a timeout and then immediately:
    	 * 		- a connection established event
    	 * 		- a connection error event
    	 * - that's why we don't do anything here
    	 * - if no subsequent event event is received, the caller will run into a timeout by default
    	 */
    	//appXDK_MQTT_ConnectionStatus = false;
        //if (pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
        break;
    case MQTT_DISCONNECT_SEND_FAILED:
    	/* test:
    	 * - not tested / observed
    	 */
    	appXDK_MQTT_ConnectionStatus = false;
        if (pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
    	break;

	/*
	 * connection closed event. it is an 'out-of-bounds' event.
	 * give (without checking) all semaphores so any outstanding calls are returned.
	 */
    case MQTT_CONNECTION_CLOSED:
    	/* test:
    	 * - delete the session
    	 * - set enabled flag to false
    	 */
    	appXDK_MQTT_ConnectionStatus = false;

    	xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle);
    	xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle);
    	xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle);
    	xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle);

        appXDK_MQTT_SetupInfo.brokerDisconnectCallback_Func();
        break;

	/*
	 * subscription handling
	 */
    case MQTT_SUBSCRIPTION_ACKNOWLEDGED:
    	appXDK_MQTT_SubscriptionStatus = true;
        if (pdTRUE != xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR);
        break;
	case MQTT_SUBSCRIBE_SEND_FAILED:
		appXDK_MQTT_SubscriptionStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR);
		break;
	case MQTT_SUBSCRIBE_TIMEOUT:
		appXDK_MQTT_SubscriptionStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR);
        break;
    case MQTT_SUBSCRIPTION_REMOVED:
    	// successful unsubscribe
		appXDK_MQTT_UnsubscribeStatus = true;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR);
        break;
    case MQTT_UNSUBSCRIBE_SEND_FAILED:
    	// failed unsubscribe
		appXDK_MQTT_UnsubscribeStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR);
    	break;
    case MQTT_UNSUBSCRIBE_TIMEOUT:
    	//Triggered if the server has not responded to the unsubscribe message.
		appXDK_MQTT_UnsubscribeStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR);
    	break;


    /*
     * incoming data handling
     */
    case MQTT_INCOMING_PUBLISH:
    	appXDK_MQTT_HandleEventIncomingDataCallback(servalEventData->publish);
        break;

    /*
     * publish data events
     */
    case MQTT_PUBLISHED_DATA:
		appXDK_MQTT_PublishStatus = true;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
		break;
    case MQTT_PUBLISH_SEND_FAILED:
    	// Triggered if sending the publish or release message caused an error
    	appXDK_MQTT_PublishStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
        break;
    case MQTT_PUBLISH_SEND_ACK_FAILED:
    	appXDK_MQTT_PublishStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
        break;
	case MQTT_PUBLISH_TIMEOUT:
		appXDK_MQTT_PublishStatus = false;
		if (pdTRUE != xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle)) appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
        break;

    /*
     * unknown events - never observed
     * may be serval internal events?
     */
	case MQTT_PING_RESPONSE_RECEIVED:
	case MQTT_PING_SEND_FAILED:
	case MQTT_SERVER_DID_NOT_RELEASE:
		appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNHANDLED_MQTT_EVENT);
		break;
    default:
    	appXDK_MQTT_EventHandler_Retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNKNOWN_CALLBACK_EVENT_RECEIVED);
        break;
    }

    // an event not initiated by the app itself but by the stack/broker
    if(!appXDK_MQTT_AppInitiatedInteraction) {
        // raise here and abort, should not happen. if it does, it's a coding error
    	if(RETCODE_OK != appXDK_MQTT_EventHandler_Retcode) {
    		Retcode_RaiseError(appXDK_MQTT_EventHandler_Retcode);
    		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED));
    	}
    }
    // reset for next event
    appXDK_MQTT_AppInitiatedInteraction = false;
    appXDK_MQTT_EventHandler_PriorServalEvent = appXDK_MQTT_EventHandler_ServalEvent;

    return RC_OK;
}
/**
 * @brief Initialize the module.
 * @details Initializes the Serval Mqtt Stack and sets the event handler @ref appXDK_MQTT_EventHandler().
 * @note Secure connection is not supported. Only supports mqttType= @ref AppXDK_MQTT_TypeServalStack
 * @param[in] setupInfoPtr: the mqtt setup information.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_INIT_FAILED) if call to Mqtt_initialize() failed
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_INIT_FAILED) if call to Mqtt_initializeInternalSession() failed
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME)
 */
Retcode_T AppXDK_MQTT_Init(const AppXDK_MQTT_Setup_T * setupInfoPtr) {

	assert(setupInfoPtr);
	assert(setupInfoPtr->brokerDisconnectCallback_Func);
	assert(setupInfoPtr->incomingDataCallBack_Func);
	assert(!setupInfoPtr->isSecure); 	// not supported (yet)
	assert(setupInfoPtr->mqttType == AppXDK_MQTT_TypeServalStack); // the only one supported

    Retcode_T retcode = RETCODE_OK;

    appXDK_MQTT_ExternalInterface_SemaphoreHandle = xSemaphoreCreateBinary();
    if (NULL == appXDK_MQTT_ExternalInterface_SemaphoreHandle) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);

	appXDK_MQTT_SubscribeSemaphoreHandle = xSemaphoreCreateBinary();
	if (NULL == appXDK_MQTT_SubscribeSemaphoreHandle) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle);

    appXDK_MQTT_UnsubscribeSemaphoreHandle = xSemaphoreCreateBinary();
	if(appXDK_MQTT_UnsubscribeSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle);

    appXDK_MQTT_PublishSemaphoreHandle = xSemaphoreCreateBinary();
	if(appXDK_MQTT_PublishSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle);

    appXDK_MQTT_ConnectSemaphoreHandle = xSemaphoreCreateBinary();
	if(appXDK_MQTT_ConnectSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle);

	appXDK_MQTT_SetupInfo = *setupInfoPtr;

    switch (appXDK_MQTT_SetupInfo.mqttType) {
        case AppXDK_MQTT_TypeServalStack: {

        	appXDK_MQTT_ServalSession.onMqttEvent = appXDK_MQTT_EventHandler;

        	if (RC_OK != Mqtt_initialize()) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_INIT_FAILED);
            if (RC_OK != Mqtt_initializeInternalSession(&appXDK_MQTT_ServalSession)) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_INIT_FAILED);
        }
        break;
        default:
        	retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
        	break;
    }

    return retcode;

}
/**
 * @brief Setup the module.
 * Placeholder at the moment.
 * Only supports non-secure communication and only Serval stack.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME)
 */
Retcode_T AppXDK_MQTT_Setup(void) {

    Retcode_T retcode = RETCODE_OK;

    switch (appXDK_MQTT_SetupInfo.mqttType) {
    case AppXDK_MQTT_TypeServalStack:
        if (appXDK_MQTT_SetupInfo.isSecure) {
        	assert(0); // todo: implement
#if SERVAL_ENABLE_TLS_CLIENT
#if SERVAL_ENABLE_TLS && SERVAL_TLS_MBEDTLS
            if(RC_OK != MbedTLSAdapter_Initialize())
            {
                printf("MbedTLSAdapter_Initialize : unable to initialize Mbedtls.\r\n");
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_HTTP_INIT_REQUEST_FAILED);
            }
#endif /* SERVAL_ENABLE_TLS && SERVAL_TLS_MBEDTLS */
            if ( RETCODE_OK == retcode ) {
                retcode = HTTPRestClientSecurity_Setup();
            }
#else /* SERVAL_ENABLE_TLS_CLIENT */
            retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_HTTP_ENABLE_SERVAL_TLS_CLIENT);
#endif /* SERVAL_ENABLE_TLS_CLIENT */
        }
        break;
    default:
        retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
        break;
    }

	return retcode;
}
/**
 * @brief Connect to the mqtt broker.
 * @details Blocks module using semaphore @ref appXDK_MQTT_ExternalInterface_SemaphoreHandle and sets #appXDK_MQTT_State to #AppXDK_MQTT_State_Connecting and to #AppXDK_MQTT_State_Ready when finished.
 * @details Uses #appXDK_MQTT_ConnectSemaphoreHandle for synchronization with @ref appXDK_MQTT_EventHandler().
 *
 * @note Only supports #AppXDK_MQTT_TypeServalStack.
 *
 * @param[in] connectPtr: the connect information
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_URL_PARSING_FAILED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_CONNECT_CALL_FAILED) when the call to Mqtt_connect() itself fails
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_CONNECT_CALLBACK_RECEIVED_FROM_BROKER) if event handler does not receive any callback in time.
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED) if #appXDK_MQTT_EventHandler_Retcode not RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_CONNECTION_ERROR_RECEIVED_FROM_BROKER) if connection to broker failed
 *
 */
Retcode_T AppXDK_MQTT_ConnectToBroker(const AppXDK_MQTT_Connect_T * connectPtr) {

	assert(connectPtr);

	Retcode_T retcode = RETCODE_OK;

	if(pdTRUE != xSemaphoreTake(appXDK_MQTT_ExternalInterface_SemaphoreHandle, MILLISECONDS(APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_CONNECT_SEMAPHORE_WAIT_IN_MS))) {
		return appXDK_MQTT_GetModuleBusyRetcode();
	}
	appXDK_MQTT_State = AppXDK_MQTT_State_Connecting;

    switch (appXDK_MQTT_SetupInfo.mqttType) {
        case AppXDK_MQTT_TypeServalStack: {

            Ip_Address_T brokerIpAddress = 0UL;
            StringDescr_T clientID;
            StringDescr_T username;
            StringDescr_T password;
            char mqttBrokerURL[30] = { 0 };
            char serverIpStringBuffer[16] = { 0 };

            if (RETCODE_OK == retcode) retcode = NetworkConfig_GetIpAddress((uint8_t *) connectPtr->brokerUrl, &brokerIpAddress);

            if (RETCODE_OK == retcode) {
                if (0 > Ip_convertAddrToString(&brokerIpAddress, serverIpStringBuffer)) retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_IP_ADDRESS_CONVERSION_FAILED);
            }
            if (RETCODE_OK == retcode) {
            	appXDK_MQTT_ServalSession.MQTTVersion = 3;
            	appXDK_MQTT_ServalSession.keepAliveInterval = connectPtr->keepAliveIntervalSecs;
            	appXDK_MQTT_ServalSession.cleanSession = connectPtr->isCleanSession;
            	appXDK_MQTT_ServalSession.will.haveWill = false;

                StringDescr_wrap(&clientID, connectPtr->clientId);
                appXDK_MQTT_ServalSession.clientID = clientID;

                StringDescr_wrap(&username, connectPtr->username);
                appXDK_MQTT_ServalSession.username = username;

                StringDescr_wrap(&password, connectPtr->password);
                appXDK_MQTT_ServalSession.password = password;

                if (appXDK_MQTT_SetupInfo.isSecure) {
                    sprintf(mqttBrokerURL, APP_XDK_MQTT_URL_FORMAT_SECURE, serverIpStringBuffer, connectPtr->brokerPort);
                    appXDK_MQTT_ServalSession.target.scheme = SERVAL_SCHEME_MQTTS;
                }
                else {
                    sprintf(mqttBrokerURL, APP_XDK_MQTT_URL_FORMAT_NON_SECURE, serverIpStringBuffer, connectPtr->brokerPort);
                    appXDK_MQTT_ServalSession.target.scheme = SERVAL_SCHEME_MQTT;
                }

                #ifdef DEBUG_APP_XDK_MQTT
               	printf("[INFO] - AppXDK_MQTT_ConnectToBroker: broker %s \r\n", mqttBrokerURL);
               	printf("[INFO] - AppXDK_MQTT_ConnectToBroker: username %s \r\n", appXDK_MQTT_ServalSession.username.start);
               	printf("[INFO] - AppXDK_MQTT_ConnectToBroker: password %s \r\n", appXDK_MQTT_ServalSession.password.start);
				#endif
                if (RC_OK != SupportedUrl_fromString((const char *) mqttBrokerURL, (uint16_t) strlen((const char *) mqttBrokerURL), &appXDK_MQTT_ServalSession.target)) {
                	retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_URL_PARSING_FAILED);
                }

                // now we connect
                if(RETCODE_OK == retcode) {

                    if (pdTRUE != xSemaphoreTake(appXDK_MQTT_ConnectSemaphoreHandle, 0UL)) {
                    	//another connect must be going on - should never happen, would be a coding error
                    	Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR));
                    } else {

                    	appXDK_MQTT_AppInitiatedInteraction = true;

                        appXDK_MQTT_ConnectionStatus = false;

                    	retcode_t servalRetcode = Mqtt_connect(&appXDK_MQTT_ServalSession);

                        if (RC_OK != servalRetcode) {
							#ifdef DEBUG_APP_XDK_MQTT
                        	printf("[ERROR] - AppXDK_MQTT_ConnectToBroker : Serval Mqtt_connect() call failed with servalRetcode=%i.\r\n", servalRetcode);
							#endif
                        	if(RC_MQTT_ALREADY_CONNECTED == servalRetcode) {
                        		appXDK_MQTT_ConnectionStatus = true;
                        	}
                            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_CONNECT_CALL_FAILED);
                        }
                    }
                }
                // now wait for the response from broker
                if (RETCODE_OK == retcode) {
					if (pdTRUE != xSemaphoreTake(appXDK_MQTT_ConnectSemaphoreHandle, pdMS_TO_TICKS(APP_XDK_MQTT_CONNECT_TIMEOUT_IN_MS))) {
						if(MQTT_CONNECT_TIMEOUT == appXDK_MQTT_EventHandler_ServalEvent) {
							// this cannot happen, the timout event does not release the semaphore
							appXDK_MQTT_ConnectionStatus = false;
							retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECTION_ERROR_RECEIVED_FROM_BROKER);
						} else {
							#ifdef DEBUG_APP_XDK_MQTT
							printf("[ERROR] - AppXDK_MQTT_ConnectToBroker : Failed, timeout waiting for response event.\r\n");
							#endif
							appXDK_MQTT_ConnectionStatus = false;
							retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_CONNECT_CALLBACK_RECEIVED_FROM_BROKER);
						}
					}
					else {
						// check the retcode of the event handler
						if (RETCODE_OK != appXDK_MQTT_EventHandler_Retcode) {
							#ifdef DEBUG_APP_XDK_MQTT
							printf("[ERROR] - AppXDK_MQTT_ConnectToBroker : appXDK_MQTT_EventHandler_Retcode: \r\n");
							#endif
							Retcode_RaiseError(appXDK_MQTT_EventHandler_Retcode);
							retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED);
						} else {
							// no error, now check the outcome
							if (!appXDK_MQTT_ConnectionStatus) {
								retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECTION_ERROR_RECEIVED_FROM_BROKER);
							}
						}
						if(pdTRUE != xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle)) {
							retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR);
						}
					}
                }
            }
        }
            break;
        default:
            retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
            break;
	}

    if(RETCODE_OK != retcode) {
    	xSemaphoreGive(appXDK_MQTT_ConnectSemaphoreHandle);
    	if(RETCODE_SEVERITY_FATAL == Retcode_GetSeverity(retcode)) Retcode_RaiseError(retcode);
    }

    appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);

    return retcode;
}
/**
 * @brief Subscribe to a topic.
 * @details Blocks module for external access and unblocks when ready again.
 *
 * @param[in] subscribeInfoPtr: subscribe info
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_SUBSCRIBE_FAILED_NO_CONNECTION) when not connected to the broker
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_SUBSCRIBE_CALL_FAILED) if call to Mqtt_subscribe() itself fails
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_SUBSCRIBE_CALLBACK_RECEIVED_FROM_BROKER)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_ERROR_RECEIVED_FROM_BROKER)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME)
 *
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED))
 */
Retcode_T AppXDK_MQTT_SubsribeToTopic(const AppXDK_MQTT_Subscribe_T * subscribeInfoPtr) {
    Retcode_T retcode = RETCODE_OK;

	if(pdTRUE != xSemaphoreTake(appXDK_MQTT_ExternalInterface_SemaphoreHandle,  MILLISECONDS(APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_SUBSCRIBE_SEMAPHORE_WAIT_IN_MS))) {
		return appXDK_MQTT_GetModuleBusyRetcode();
	}

	appXDK_MQTT_State = AppXDK_MQTT_State_Subscribing;

    assert(subscribeInfoPtr);
    assert(subscribeInfoPtr->topic);

    if(!appXDK_MQTT_ConnectionStatus) {
    	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
    	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);
    	return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_SUBSCRIBE_FAILED_NO_CONNECTION);
    }

	#ifdef DEBUG_APP_XDK_MQTT
	printf("[INFO] - AppXDK_MQTT_SubsribeToTopic : Subscribing to topic: %s, Qos: %u\r\n", subscribeInfoPtr->topic, subscribeInfoPtr->qos);
	#endif

    switch (appXDK_MQTT_SetupInfo.mqttType) {

    case AppXDK_MQTT_TypeServalStack: {

    	static StringDescr_T subscribeTopicDescription[1];
		static Mqtt_qos_t qos[1];

		StringDescr_wrap(&(subscribeTopicDescription[0]), subscribeInfoPtr->topic);
		qos[0] = (Mqtt_qos_t) subscribeInfoPtr->qos;

        if (pdTRUE != xSemaphoreTake(appXDK_MQTT_SubscribeSemaphoreHandle, 0UL)) {
        	//another subscribe must be going on - should never happen, it's a coding error
        	retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR);
        	Retcode_RaiseError(retcode);
        } else {

        	appXDK_MQTT_AppInitiatedInteraction = true;

    		appXDK_MQTT_SubscriptionStatus = false;

    		if (RC_OK != Mqtt_subscribe(&appXDK_MQTT_ServalSession, 1, subscribeTopicDescription, qos)) {
				#ifdef DEBUG_APP_XDK_MQTT
            	printf("[ERROR] - AppXDK_MQTT_SubsribeToTopic : Serval Mqtt_subscribe() call failed.\r\n");
				#endif
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_SUBSCRIBE_CALL_FAILED);
            }
        }

        // now wait for the callback
        if (RETCODE_OK == retcode) {

        	if (pdTRUE != xSemaphoreTake(appXDK_MQTT_SubscribeSemaphoreHandle, pdMS_TO_TICKS(APP_XDK_MQTT_SUBSCRIBE_TIMEOUT_IN_MS))) {
				#ifdef DEBUG_APP_XDK_MQTT
				printf("[ERROR] - AppXDK_MQTT_SubsribeToTopic : Failed, never received any SUBSCRIBE_XXX event.\r\n");
				#endif
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_SUBSCRIBE_CALLBACK_RECEIVED_FROM_BROKER);
			}
			else {
				// check the retcode of the event handler
				if (RETCODE_OK != appXDK_MQTT_EventHandler_Retcode) {
					#ifdef DEBUG_APP_XDK_MQTT
					printf("[ERROR] - AppXDK_MQTT_SubsribeToTopic : appXDK_MQTT_EventHandler_Retcode: \r\n");
					#endif
					Retcode_RaiseError(appXDK_MQTT_EventHandler_Retcode);
					Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED));
				}
				// no error, now check the outcome
				if (true != appXDK_MQTT_SubscriptionStatus) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_ERROR_RECEIVED_FROM_BROKER);
				}
				if(pdTRUE != xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle)) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR);
				}
			}
		}
	}
        break;
	default:
		retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
        break;
    }

    if(RETCODE_OK != retcode) xSemaphoreGive(appXDK_MQTT_SubscribeSemaphoreHandle);

	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);

    return retcode;
}
/**
 * @brief Unsubscribe from multiple topics in one call.
 * @details Blocks / unblocks module for external access.
 *
 * @param[in] numTopics: the number of topics in the array
 * @param[in] topicsStrArray: the topic string array
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_UNSUBSCRIBE_FAILED_NO_CONNECTION)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_UNSUBSCRIBE_CALL_FAILED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_ERROR_RECEIVED_FROM_BROKER)
 */
Retcode_T AppXDK_MQTT_UnsubsribeFromTopics(const uint8_t numTopics, const char * topicsStrArray[]) {
    Retcode_T retcode = RETCODE_OK;

	if(pdTRUE != xSemaphoreTake(appXDK_MQTT_ExternalInterface_SemaphoreHandle,  MILLISECONDS(APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_UNSUBSCRIBE_SEMAPHORE_WAIT_IN_MS))) {
		return appXDK_MQTT_GetModuleBusyRetcode();
	}

	appXDK_MQTT_State = AppXDK_MQTT_State_Unsubscribing;

    assert(topicsStrArray);
    assert(numTopics <= APP_XDK_MQTT_MAX_UNSUBSCRIBE_COUNT);

    if(!appXDK_MQTT_ConnectionStatus) {
    	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
    	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);
    	return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_UNSUBSCRIBE_FAILED_NO_CONNECTION);
    }

    switch (appXDK_MQTT_SetupInfo.mqttType) {
	case AppXDK_MQTT_TypeServalStack: {

		static StringDescr_T topicDescrArray[APP_XDK_MQTT_MAX_UNSUBSCRIBE_COUNT];

		for(int i=0; i < numTopics; i++) {
			StringDescr_wrap(&(topicDescrArray[i]), topicsStrArray[i]);
			#ifdef DEBUG_APP_XDK_MQTT
			printf("[INFO] - AppXDK_MQTT_UnsubsribeFromTopics : topic[%i]: %s\r\n", i, topicsStrArray[i]);
			#endif
		}

        if (pdTRUE != xSemaphoreTake(appXDK_MQTT_UnsubscribeSemaphoreHandle, 0UL)) {
        	//another unsubscribe must be going on - should never happen, it's a coding error
        	retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR);
        	Retcode_RaiseError(retcode);
        } else {

        	appXDK_MQTT_AppInitiatedInteraction = true;

    		appXDK_MQTT_UnsubscribeStatus = false;

    		if(RC_OK != Mqtt_unsubscribe(&appXDK_MQTT_ServalSession, numTopics, topicDescrArray)) {
				#ifdef DEBUG_APP_XDK_MQTT
            	printf("[ERROR] - AppXDK_MQTT_UnsubsribeFromTopics : Serval Mqtt_unsubscribe() call failed.\r\n");
				#endif
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_UNSUBSCRIBE_CALL_FAILED);
            }
        }

        // wait for the callback
		if (RETCODE_OK == retcode) {
			if (pdTRUE != xSemaphoreTake(appXDK_MQTT_UnsubscribeSemaphoreHandle, pdMS_TO_TICKS(APP_XDK_MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS))) {
				#ifdef DEBUG_APP_XDK_MQTT
				printf("[ERROR] - AppXDK_MQTT_UnsubsribeFromTopics : Failed, never received any UNSUBSCRIBE_XXX event.\r\n");
				#endif
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_UNSUBSCRIBE_CALLBACK_RECEIVED_FROM_BROKER);
			} else {
				// check the retcode of the event handler
				if (RETCODE_OK != appXDK_MQTT_EventHandler_Retcode) {
					#ifdef DEBUG_APP_XDK_MQTT
					printf("[ERROR] - AppXDK_MQTT_UnsubsribeFromTopics : appXDK_MQTT_EventHandler_Retcode: \r\n");
					#endif
					Retcode_RaiseError(appXDK_MQTT_EventHandler_Retcode);
					Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED));
				}
				// no error, now check the outcome
				if (true != appXDK_MQTT_UnsubscribeStatus) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_ERROR_RECEIVED_FROM_BROKER);
				}
				if(pdTRUE != xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle)) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR);
				}
			}
		}
	}
		break;
	default:
		retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
		break;
    }

    if(RETCODE_OK != retcode) xSemaphoreGive(appXDK_MQTT_UnsubscribeSemaphoreHandle);

	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);

    return retcode;
}
/**
 * @brief Publish a message.
 *
 * @details Waits different lenghts of time for qos=0 and qos=1 (#APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_0_SEMAPHORE_WAIT_IN_MS, #APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_1_SEMAPHORE_WAIT_IN_MS)
 *
 * @note This does not seem to work correctly for QoS=1 publishing. Frequently receives a disconnect event.
 *
 * @param[in] publishPtr: the publish information
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_PUBLISH_CALL_FAILED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_PUBLISH_CALLBACK_RECEIVED_FROM_BROKER)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_FAILED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME)
 *
 */
Retcode_T AppXDK_MQTT_PublishToTopic(const AppXDK_MQTT_Publish_T * publishPtr) {

	assert(publishPtr);
	assert(publishPtr->topic);
	assert(publishPtr->payload);
	assert(publishPtr->payloadLength > 0);

	uint32_t waitMillis = (publishPtr->qos==0) ? APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_0_SEMAPHORE_WAIT_IN_MS : APP_XDK_MQTT_TAKE_EXTERNAL_INTERFACE_PUBLISH_QOS_1_SEMAPHORE_WAIT_IN_MS;
	if(pdTRUE != xSemaphoreTake(appXDK_MQTT_ExternalInterface_SemaphoreHandle,  MILLISECONDS(waitMillis))) {
		// note: this should not happen, if it does, raise fatal error
		if(publishPtr->qos==1) Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_FAILED_FOR_QOS_1));
		return appXDK_MQTT_GetModuleBusyRetcode();
	}

	appXDK_MQTT_State = AppXDK_MQTT_State_Publishing;

    Retcode_T retcode = RETCODE_OK;

    if(!appXDK_MQTT_ConnectionStatus) {
    	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
    	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);
    	return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_PUBLISH_FAILED_NO_CONNECTION);
    }

	switch (appXDK_MQTT_SetupInfo.mqttType) {

	case AppXDK_MQTT_TypeServalStack: {

		static StringDescr_T publishTopicDescription;
		StringDescr_wrap(&publishTopicDescription, publishPtr->topic);

        if (pdTRUE != xSemaphoreTake(appXDK_MQTT_PublishSemaphoreHandle, 0UL)) {
        	//another publish must be going on - should never happen, it's a coding error
        	retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
        	Retcode_RaiseError(retcode);
        } else {

        	appXDK_MQTT_AppInitiatedInteraction = true;

    		appXDK_MQTT_PublishStatus = false;

    		if (RC_OK != Mqtt_publish(&appXDK_MQTT_ServalSession, publishTopicDescription, publishPtr->payload, publishPtr->payloadLength, (uint8_t) publishPtr->qos, false)) {
				#ifdef DEBUG_APP_XDK_MQTT
            	printf("[ERROR] - AppXDK_MQTT_PublishToTopic : Serval Mqtt_publish() call failed.\r\n");
				#endif
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_PUBLISH_CALL_FAILED);
            }
        }

        // wait for the callback
        if (RETCODE_OK == retcode) {
			if (pdTRUE != xSemaphoreTake(appXDK_MQTT_PublishSemaphoreHandle, pdMS_TO_TICKS(APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS))) {
				#ifdef DEBUG_APP_XDK_MQTT
				printf("[ERROR] - AppXDK_MQTT_PublishToTopic : Failed, never received any PUBLISH_XXX event.\r\n");
				#endif
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_PUBLISH_CALLBACK_RECEIVED_FROM_BROKER);
			}
			else {
				// check the retcode of the event handler
				if (RETCODE_OK != appXDK_MQTT_EventHandler_Retcode) {
					#ifdef DEBUG_APP_XDK_MQTT
					printf("[ERROR] - AppXDK_MQTT_PublishToTopic : appXDK_MQTT_EventHandler_Retcode: \r\n");
					#endif
					Retcode_RaiseError(appXDK_MQTT_EventHandler_Retcode);
					Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED));
				}
				// no error, now check the outcome
				if (true != appXDK_MQTT_PublishStatus) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_FAILED);
				}
				if(pdTRUE != xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle)) {
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR);
				}
			}
		}
	}
		break;
	default:
		retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME);
		break;
	}

    if(RETCODE_OK != retcode) xSemaphoreGive(appXDK_MQTT_PublishSemaphoreHandle);

	appXDK_MQTT_State = AppXDK_MQTT_State_Ready;
	xSemaphoreGive(appXDK_MQTT_ExternalInterface_SemaphoreHandle);

    return retcode;
}


/**@}*/
/** ************************************************************************* */



