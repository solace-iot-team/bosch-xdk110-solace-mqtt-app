/*
 * AppStatus.c
 *
 *  Created on: 14 Aug 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppStatus AppStatus
 * @{
 *
 * @brief Manages the status API.
 *
 * @details Provides functions for modules to send status messages. Implements an in-memory queue (array) for status messages in case we have no connection to the broker. These will be sent once connection is (re-)established.
 * @details Can be configured to send periodic status messages.
 * @details Provides the management of Retcode_RaiseError().
 * @details Provides functions for collecting stats.
 * @details Module runs in its own command processor.
 *
 * @author $(SOLACE_APP_AUTHOR)
 *
 * @date $(SOLACE_APP_DATE)
 *
 * @file
 *
 *
 **/

#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_STATUS

#include "AppStatus.h"
#include "AppMisc.h"
#include "AppMqtt.h"

#include "BCDS_Assert.h"
#include "BCDS_BSP_Board.h"
#include "task.h"
#include "BatteryMonitor.h"


#define APP_STATUS_QUEUE_TO_SEND_MAX				(UINT8_C(10)) /**< max number of status messages queued in memory in case of no connection to broker */
static cJSON * appStatus_JsonQueue2SendPtrArray[APP_STATUS_QUEUE_TO_SEND_MAX]; /**< the queue of status messages to be sent when connected to broker again */
static uint8_t appStatus_JsonQueue2Send_NextIndex = 0; /**< the index of the last queued message */
#define APP_STATUS_QUEUE2SEND_WAIT_TICKS_MS			(UINT32_C(100)) /**< wait millis between sending queued messages to avoid disconnect */
static SemaphoreHandle_t appStatus_JsonQueue_SemaphoreHandle = NULL; /**< queue semaphore */
#define APP_STATUS_JSON_QUEUE_SEMAPHORE_TAKE_ADD_WAIT_TICKS_MS		(UINT32_C(10000)) /**< wait millis for adding a message to the queue */
#define APP_STATUS_JSON_QUEUE_SEMAPHORE_TAKE_SEND_WAIT_TICKS_MS		(UINT32_C(10)) /**< wait millis for sending messages from the queue */

/**
 * @brief The default publish info for status messages.
 */
static AppXDK_MQTT_Publish_T appStatus_MqttPublishInfo = {
	.topic = NULL,
	.qos = 1UL,
	.payload = NULL,
	.payloadLength = 0UL,
};
static SemaphoreHandle_t appStatus_MqttPublishInfo_SemaphoreHandle = NULL; /**< semaphore to protect publish info */

static const CmdProcessor_T * appStatus_ProcessorHandle = NULL; /**< the status module command processor.*/
static xTaskHandle appStatus_TaskHandle = NULL; /**< the task handle for periodic status messages */
static SemaphoreHandle_t appStatus_TaskSemaphoreHandle = NULL; /**< the semaphore to protect the periodic send task */

static bool appStatus_isEnabled = false; /**< internal flag to indicate whether module is enabled. If set to false, messages will be queued */

static uint32_t appStatus_LastStatusSentTicks = 0; /**< internal tick counter to synchronize period status messages sending */

static SemaphoreHandle_t appStatus_ErrorHandlingFunc_SemaphoreHandle = NULL; /**< semaphore to protect @ref AppStatus_ErrorHandlingFunc() */
/**
 * @brief Timeout for semaphore take in @ref AppStatus_ErrorHandlingFunc().
 * Greater than the publishing timeout, since the function may publish the error.
 */
#define APP_STATUS_ERROR_HANDLING_FUNC_SEMAPHORE_TAKE_WAIT_IN_MS 	(APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS * 2)

/**
 * @brief Structure for stats.
 */
typedef struct {
	uint32_t mqttBrokerDisconnectCounter; /**< number of mqtt broker disconnects since boot */
	uint32_t wlanDisconnectCounter; /**< number of WLAN disconnects since boot */
	uint32_t statusSendFailedCounter; /**< number of status messages failed to send */
	uint32_t telemetrySendFailedCounter; /**< number of telemetry messages failed to send */
	uint32_t telemetrySendTooSlowCounter; /**< number of telemetry messages publish too slow */
	uint32_t telemetrySamplingTooSlowCounter; /**< number of telemetry sampling cycles missed */
	uint32_t retcodeRaisedErrorCounter; /**< number of errors passed through #Retcode_RaiseError() to #AppStatus_ErrorHandlingFunc() */
	char * bootTimestampStr; /**< the boot timestamp string */
	uint32_t bootBatteryVoltage; /**< boot battery voltage */
	uint32_t currentBatteryVoltage; /**< current battery voltage */
} AppStatus_Stats_T;
/**
 * @brief Stats.
 */
static AppStatus_Stats_T appStatus_Stats = {
	.mqttBrokerDisconnectCounter = 0,
	.wlanDisconnectCounter = 0,
	.statusSendFailedCounter = 0,
	.telemetrySendFailedCounter = 0,
	.telemetrySamplingTooSlowCounter = 0,
	.retcodeRaisedErrorCounter = 0,
	.bootTimestampStr = NULL,
	.bootBatteryVoltage = 0,
	.currentBatteryVoltage = 0,
};
static SemaphoreHandle_t appStatus_Stats_SemaphoreHandle = NULL; /**< semaphore to protect access to #appStatus_Stats */
#define APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS		(UINT32_C(100)) /**< wait in millis to take stats semaphore */

static bool appStatus_isPeriodicStatus = false; /**< flag to indicate if module sends out periodic status messages. this is a local copy of the configuration. */

static AppRuntimeConfig_PeriodicStatusType_T appStatus_PeriodicStatusType = APP_RT_CFG_DEFAULT_STATUS_PERIODIC_TYPE; /**< local copy of configuration. the type of periodic status messages */

static TickType_t appStatus_PeriodicStatusIntervalMillis = SECONDS(APP_RT_CFG_DEFAULT_STATUS_INTERVAL_SECS); /**< the periodic interval in millis to send status messages */

static const char * appStatus_DeviceId = NULL; /**< local copy of the device id */

/* forward declarations */
static cJSON * appStatus_Stats_GetAsJson(void);
static void appStatus_CreateRecurringTask(void);
static void appStatus_Stats_IncrementMqttBrokerDisconnectCounter(void);
static void appStatus_Stats_IncrementWlanDisconnectCounter(void);
static void appStatus_Stats_IncrementStatusSendFailedCounter(void);
static void appStatus_Stats_IncrementTelemetrySendFailedCounter(void);
static void appStatus_Stats_IncrementTelemetrySendTooSlowCounter(void);
static void appStatus_Stats_IncrementTelemetrySamplingTooSlowCounter(void);
static void appStatus_Stats_IncrementRetcodeRaisedErrorCounter(void);
static void appStatus_SetStatusConfig(AppRuntimeConfig_StatusConfig_T * statusConfigPtr);
static void appStatus_SetPubTopic(AppRuntimeConfig_TopicConfig_T const * const topicConfigPtr);
static void appStatus_RecurringSendTask(void* pvParameters);
static void appStatus_DeleteMessage(AppStatusMessage_T * statusMessage);
static void appStatus_SendQueuedMessages(void);
static void appStatus_QueueJson4Sending(cJSON * jsonHandle);
static cJSON * appStatus_GetRetcodeAsJson(const Retcode_T retcode);


/**
 * @brief Initialize the module.
 * Calls @ref BatteryMonitor_Init() and initializes the stat @ref appStatus_Stats.bootBatteryVoltage.
 *
 * @param[in] deviceId : the device id. keeps a local copy.
 * @param[in] processorHandle : the processor handle for this module. keeps a local copy.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 * @return Retcode_T: return code from @ref BatteryMonitor_Init() or @ref BatteryMonitor_MeasureSignal()
 *
 */
Retcode_T AppStatus_Init(const char * deviceId, const CmdProcessor_T * processorHandle) {

	assert(deviceId);
	assert(processorHandle);

	Retcode_T retcode = RETCODE_OK;

	appStatus_ProcessorHandle = processorHandle;

	appStatus_DeviceId = copyString(deviceId);

	appStatus_TaskSemaphoreHandle = xSemaphoreCreateBinary();
	if(appStatus_TaskSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appStatus_TaskSemaphoreHandle);

	appStatus_JsonQueue_SemaphoreHandle = xSemaphoreCreateBinary();
	if(appStatus_JsonQueue_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appStatus_JsonQueue_SemaphoreHandle);

	appStatus_MqttPublishInfo_SemaphoreHandle = xSemaphoreCreateBinary();
	if(appStatus_MqttPublishInfo_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appStatus_MqttPublishInfo_SemaphoreHandle);

	// should have been initialized previously with AppStatus_InitErrorHandling()
	if(NULL == appStatus_Stats_SemaphoreHandle) {
		appStatus_Stats_SemaphoreHandle = xSemaphoreCreateBinary();
		if(appStatus_Stats_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
	}

	if (RETCODE_OK == retcode) retcode = BatteryMonitor_Init();

	if (RETCODE_OK == retcode) retcode = BatteryMonitor_MeasureSignal(&appStatus_Stats.bootBatteryVoltage);

	return retcode;

}
/**
 * @brief Setup the module.
 * Sets up internal configuration for status & topic.
 *
 * @param[in] configPtr : the runtime config
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppStatus_Setup(const AppRuntimeConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->topicConfigPtr);
	assert(configPtr->statusConfigPtr);

	Retcode_T retcode = RETCODE_OK;

	appStatus_SetStatusConfig(configPtr->statusConfigPtr);

	appStatus_SetPubTopic(configPtr->topicConfigPtr);

	return retcode;
}
/**
 * @brief Enable the module.
 * Sets the boot timestamp @ref appStatus_Stats.bootTimestampStr
 *
 * @return Retcode_T : RETCODE_OK
 */
Retcode_T AppStatus_Enable(AppTimestamp_T bootTimestamp) {

	Retcode_T retcode = RETCODE_OK;

	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) )) {
		assert(appStatus_Stats.bootTimestampStr == NULL);
		appStatus_Stats.bootTimestampStr = AppTimestamp_CreateTimestampStr(bootTimestamp);
		xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
	} else assert(0);

	appStatus_isEnabled = true;

	return retcode;
}
/**
 * @brief Apply a new configuration to the module.
 * @details Suspends the recurring task if it is running. Applies the new configuration and re-starts the task if so configured.
 *
 * @param[in] configElement : the config element type, either #AppRuntimeConfig_Element_topicConfig or #AppRuntimeConfig_Element_statusConfig
 * @param[in] configPtr : the new configuration
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T:  RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 * @return Retcode_T: return from @ref AppStatus_SuspendRecurringTask()
 *
 * **Example Usage**
 * @code
 *
 * 	if (RETCODE_OK == retcode) retcode = AppStatus_SuspendRecurringTask();
 *
 * 	... apply other configurations
 *
 * 	if (RETCODE_OK == retcode) retcode = AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_statusConfig, getAppRuntimeConfigPtr()->statusConfigPtr);
 *
 * 	or
 *
 *  if (RETCODE_OK == retcode) retcode = AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, getAppRuntimeConfigPtr()->topicConfigPtr);
 *
 * @endcode
 */
Retcode_T AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * const configPtr) {

	assert(configPtr);

	Retcode_T retcode = RETCODE_OK;

	retcode = AppStatus_SuspendRecurringTask();

	if(RETCODE_OK != retcode) return retcode;

	switch(configElement) {
		case AppRuntimeConfig_Element_topicConfig:

			appStatus_SetPubTopic((AppRuntimeConfig_TopicConfig_T *)configPtr);

			break;
		case AppRuntimeConfig_Element_statusConfig:

			appStatus_SetStatusConfig((AppRuntimeConfig_StatusConfig_T * )configPtr);

			break;
		default: retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);
	}

	if(RETCODE_OK == retcode) {
		if(appStatus_isPeriodicStatus) appStatus_CreateRecurringTask();
	}
	return retcode;
}
/**
 * @brief Call to notify module that broker is disconnected.
 * @details Sets module to 'not enabled' and suspends the recurring task.
 * @return Retcode_T: return from @ref AppStatus_SuspendRecurringTask()
 */
Retcode_T AppStatus_NotifyDisconnectedFromBroker(void) {

	appStatus_isEnabled = false;

	Retcode_T retcode = AppStatus_SuspendRecurringTask();

	return retcode;
}
/**
 * @brief Call to notify module that broker is reconnected.
 * @details Sets module to enabled, sends all queued messages and starts recurring tasks if configured.
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppStatus_NotifyReconnected2Broker(void) {

	appStatus_isEnabled = true;

	appStatus_SendQueuedMessages();

	if(appStatus_isPeriodicStatus) appStatus_CreateRecurringTask();

	return RETCODE_OK;
}
/**
 * @brief Copies the relevant config from statusConfigPtr.
 * @param[in] statusConfigPtr: the status configuration
 */
static void appStatus_SetStatusConfig(AppRuntimeConfig_StatusConfig_T * statusConfigPtr) {

	// make sure we are not publishing
	if(pdTRUE == xSemaphoreTake(appStatus_MqttPublishInfo_SemaphoreHandle,  MILLISECONDS(APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS))) {

		appStatus_MqttPublishInfo.qos = statusConfigPtr->received.qos;
		appStatus_isPeriodicStatus = statusConfigPtr->received.isSendPeriodicStatus;
		appStatus_PeriodicStatusType = statusConfigPtr->received.periodicStatusType;
		appStatus_PeriodicStatusIntervalMillis = SECONDS(statusConfigPtr->received.periodicStatusIntervalSecs);

		xSemaphoreGive(appStatus_MqttPublishInfo_SemaphoreHandle);

	} else assert(0);

}
/**
 * @brief Set the topic to publish status messages on based on the configuration.
 * @param[in] topicConfigPtr: the topic configuration
 */
static void appStatus_SetPubTopic(AppRuntimeConfig_TopicConfig_T const * const topicConfigPtr) {

	assert(topicConfigPtr);

	// make sure we are not publishing
	if(pdTRUE == xSemaphoreTake(appStatus_MqttPublishInfo_SemaphoreHandle, MILLISECONDS(APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS))) {

		if(appStatus_MqttPublishInfo.topic) free(appStatus_MqttPublishInfo.topic);
		appStatus_MqttPublishInfo.topic = AppMisc_FormatTopic("%s/iot-control/%s/device/%s/status",
													topicConfigPtr->received.methodUpdate,
													topicConfigPtr->received.baseTopic,
													appStatus_DeviceId);

		xSemaphoreGive(appStatus_MqttPublishInfo_SemaphoreHandle);

	} else assert(0);
}
/**
 * @brief Returns the status message as a JSON.
 * @param[in] msgPtr: the status message
 * @return cJSON *: the JSON
 */
static cJSON * appStatus_GetStatusMessageAsJson(const AppStatusMessage_T * msgPtr) {

	assert(msgPtr);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON_AddItemToObject(jsonHandle, "deviceId", cJSON_CreateString(appStatus_DeviceId));

	char * timestampStr = AppTimestamp_CreateTimestampStr(msgPtr->createdTimestamp);
	if(timestampStr != NULL) {
		cJSON_AddItemToObject(jsonHandle, "timestamp", cJSON_CreateString(timestampStr));
		free(timestampStr);
	} else {
		// add the tickCount instead
		cJSON_AddNumberToObject(jsonHandle, "timestampTickCount", msgPtr->createdTimestamp.tickCount);
	}

	if(msgPtr->exchangeId != NULL) cJSON_AddItemToObject(jsonHandle, "exchangeId", cJSON_CreateString(msgPtr->exchangeId));

	if(msgPtr->isManyParts) {
		cJSON_AddBoolToObject(jsonHandle, "isManyParts", msgPtr->isManyParts);
		cJSON_AddNumberToObject(jsonHandle, "totalNumberOfParts", msgPtr->totalNumParts);
		cJSON_AddNumberToObject(jsonHandle, "thisPartNumber", msgPtr->thisPartNum);
	}

	cJSON_AddNumberToObject(jsonHandle, "statusCode", msgPtr->statusCode);

	if(msgPtr->type == AppStatusMessage_Type_CmdCtrl) {
		// for backwards compatibility
		if(msgPtr->statusCode == AppStatusMessage_Status_Success) cJSON_AddItemToObject(jsonHandle, "status", cJSON_CreateString("SUCCESS"));
		else if(msgPtr->statusCode == AppStatusMessage_Status_Failed) cJSON_AddItemToObject(jsonHandle, "status", cJSON_CreateString("FAILED"));
		else assert(0);

		if(msgPtr->cmdCtrlRequestType == AppCmdCtrl_RequestType_Configuration) cJSON_AddItemToObject(jsonHandle, "requestType", cJSON_CreateString("CONFIGURATION"));
		else if(msgPtr->cmdCtrlRequestType == AppCmdCtrl_RequestType_Command) cJSON_AddItemToObject(jsonHandle, "requestType", cJSON_CreateString("COMMAND"));
		else assert(0);
	}

	cJSON_AddNumberToObject(jsonHandle, "descrCode", msgPtr->descrCode);

	if(msgPtr->details) cJSON_AddItemToObject(jsonHandle, "details", cJSON_CreateString(msgPtr->details));

	if(msgPtr->items) cJSON_AddItemToObject(jsonHandle, "items", cJSON_Duplicate(msgPtr->items, true));

	if(msgPtr->tags) cJSON_AddItemToObject(jsonHandle, "tags", cJSON_Duplicate(msgPtr->tags, true));

	return jsonHandle;

}
/**
 * @brief Internal function to send a JSON status message.
 * @details Only function that actually sends the status message. If it is a queued message, will not queue it again if module is not enabled or broker disconnected.
 * If not a queued message, will queue it if module is not enabled or broker is disconnected.
 * @param[in] jsonHandle: the JSON to send
 * @param[in] isQueuedMsg: flag to indicate if the message to send is a queued message. if yes, it won't be queued again
 * @param[in] doSendQueuedMsgsFirst: flag to indicate if function should send all queued messages first before sending the new message
 *
 * @return Retcode_T: RETCODE_OK
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_PUB_INFO_ERROR)
 *
 */
static Retcode_T appStatus_SendJsonMessage(cJSON * jsonHandle, bool isQueuedMsg, bool doSendQueuedMsgsFirst) {

	assert(jsonHandle);

	Retcode_T retcode = RETCODE_OK;

	if(!isQueuedMsg) {

		if(!appStatus_isEnabled || !AppMqtt_IsConnected()) {
			appStatus_QueueJson4Sending(jsonHandle);
			return RETCODE_OK;
		}
		// now send queued messages first if any
		if(doSendQueuedMsgsFirst) appStatus_SendQueuedMessages();

	} else {
		// leave message in queue
		if(!appStatus_isEnabled || !AppMqtt_IsConnected()) {
			return RETCODE_OK;
		}
	}

    // wait longer than AppMqtt_Publish() could take
	if(pdTRUE == xSemaphoreTake(appStatus_MqttPublishInfo_SemaphoreHandle, MILLISECONDS(APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS + 1000 ))) {

		//now check if we need to calculate the timestamp
		if(cJSON_GetObjectItem(jsonHandle, "timestamp") == NULL) {
			cJSON * timestampTickCountJsonHandle = cJSON_GetObjectItem(jsonHandle, "timestampTickCount");
			assert(timestampTickCountJsonHandle);
			assert(timestampTickCountJsonHandle->type == cJSON_Number);
			uint64_t tickCount = timestampTickCountJsonHandle->valuedouble;
			AppTimestamp_T ts;
			ts.isTickCount = true;
			ts.tickCount = tickCount;
			char * timestampStr = AppTimestamp_CreateTimestampStr(ts);
			assert(timestampStr);
			cJSON_AddItemToObject(jsonHandle, "timestamp", cJSON_CreateString(timestampStr));
			cJSON_DeleteItemFromObject(jsonHandle, "timestampTickCount");
			free(timestampStr);
		}

		char * payloadStr = cJSON_PrintUnformatted(jsonHandle);

		appStatus_MqttPublishInfo.payload = payloadStr;
		appStatus_MqttPublishInfo.payloadLength = strlen(payloadStr);

		retcode = AppMqtt_Publish(&appStatus_MqttPublishInfo);
		if(RETCODE_OK != retcode) {
			if(!isQueuedMsg) appStatus_QueueJson4Sending(jsonHandle);
			else cJSON_Delete(jsonHandle);
		} else {
		    cJSON_Delete(jsonHandle);
		}
		// it still worked
		retcode = RETCODE_OK;

		free(payloadStr);

		xSemaphoreGive(appStatus_MqttPublishInfo_SemaphoreHandle);

	} else {
		// should never happen. if it does: coding error
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_PUB_INFO_ERROR));
	}

	return retcode;
}
/**
 * @brief Send the status message and deletes it.
 * @param[in] msg: the status message
 * @return Retcode_T: retcode from @ref appStatus_SendJsonMessage()
 */
static Retcode_T appStatus_SendStatusMessage(AppStatusMessage_T * msg) {

    assert(msg);

	cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);

    Retcode_T retcode = appStatus_SendJsonMessage(jsonHandle, false, true);

	appStatus_DeleteMessage(msg);

	return retcode;
}
/**
 * @brief Wrapper to send the status message from an enqueued function in command processor.
 * Calls @ref appStatus_SendStatusMessage()
 * @exception Retcode_RaiseError: retcode from @ref appStatus_SendStatusMessage() if not RETCODE_OK
 */
static void appStatus_SendStatusMessageEnqueue(void * statusMessage, uint32_t param2) {

    BCDS_UNUSED(param2);
    assert(statusMessage);

    Retcode_T retcode = appStatus_SendStatusMessage((AppStatusMessage_T*) statusMessage);
    if(RETCODE_OK != retcode) Retcode_RaiseError(retcode);
}
/**
 * @brief Adds a copy of exchangeIdStr to the status message
 * @param[in,out] msgPtr: the status message
 * @param[in] exchangeIdStr: the exchange id
 */
static void appStatus_AddExchangeId(AppStatusMessage_T * msgPtr, const char * exchangeIdStr) {
	assert(msgPtr);
	assert(exchangeIdStr);
	msgPtr->exchangeId = copyString(exchangeIdStr);
}
/**
 * @brief Prepares the status message to be a part of a sequence of status messages.
 * @param[in,out] msgPtr: the status message
 * @param[in] totalNumParts: the total number of parts in the sequence
 * @param[in] thisPartNum: the part number of this status message in the sequence
 */
static void appStatus_AddParts(AppStatusMessage_T * msgPtr, uint8_t totalNumParts, uint8_t thisPartNum) {
	assert(msgPtr);
	msgPtr->isManyParts = true;
	msgPtr->totalNumParts = totalNumParts;
	msgPtr->thisPartNum = thisPartNum;
}
/**
 * @brief Creates a new, empty status message apart from setting the timestamp from current tick count.
 * @return AppStatusMessage_T *: the new status message
 */
static AppStatusMessage_T * appStatus_CreateMessage(void) {
	// do this first
	AppTimestamp_T ts = AppTimestamp_GetTimestamp(xTaskGetTickCount());

	AppStatusMessage_T * msg = (AppStatusMessage_T * ) malloc(sizeof(AppStatusMessage_T));
	assert(msg);

	msg->type = AppStatusMessage_Type_NULL;
	msg->cmdCtrlRequestType = AppCmdCtrl_RequestType_NULL;
	msg->statusCode = AppStatusMessage_Status_NULL;
	msg->descrCode = AppStatusMessage_Descr_NULL;
	msg->createdTimestamp = ts;
	msg->details = NULL;
	msg->items = NULL;
	msg->tags = NULL;
	msg->exchangeId = NULL;
	msg->isManyParts = false;
	msg->thisPartNum = 0;
	msg->totalNumParts = 1;

	return msg;

}
/**
 * @brief Create a new message with statusCode, descrCode and details pre-set. Takes a copy of details.
 * @param[in] statusCode: the status code
 * @param[in] descrCode: the description code
 * @param[in] details: the details string, can be NULL
 * @return AppStatusMessage_T *: the newly created status message
 */
AppStatusMessage_T * AppStatus_CreateMessage(AppStatusMessage_StatusCode_T statusCode, AppStatusMessage_DescrCode_T descrCode, const char * details) {

	AppStatusMessage_T * msg = appStatus_CreateMessage();

	msg->type = AppStatusMessage_Type_Status;
	msg->statusCode = statusCode;
	msg->descrCode = descrCode;

	if(NULL != details) msg->details = copyString(details);

	return msg;
}
/**
 * @brief Create a new message pre-set with the parameters passed.
 * @param[in] statusCode: the status code
 * @param[in] descrCode: the description code
 * @param[in] details: the details string, can be NULL
 * @param[in] exchangeId: the exchangeId, must not be NULL
 * @return AppStatusMessage_T *: the newly created status message
 */
AppStatusMessage_T * AppStatus_CreateMessageWithExchangeId(AppStatusMessage_StatusCode_T statusCode, AppStatusMessage_DescrCode_T descrCode, const char * details, const char * exchangeId) {
	AppStatusMessage_T * msg = AppStatus_CreateMessage(statusCode, descrCode, details);
	appStatus_AddExchangeId(msg, exchangeId);
	return msg;
}
/**
 * @brief Create a new Command/Control status message pre-set with the requestType.
 * @param[in] requestType: the request type of the instruction
 * @return AppStatusMessage_T *: the newly created status message
 */
AppStatusMessage_T * AppStatus_CmdCtrl_CreateMessage(AppCmdCtrlRequestType_T requestType) {
	AppStatusMessage_T * msg = appStatus_CreateMessage();
	msg->type = AppStatusMessage_Type_CmdCtrl;
	msg->cmdCtrlRequestType = requestType;
	return msg;
}
/**
 * @brief Adds a copy of the exchange Id to the status message.
 * @param[in] msg: the status message
 * @param[in,out] exchangeId: the exchange id
 */
void AppStatus_CmdCtrl_AddExchangeId(AppStatusMessage_T * msg, const char * exchangeId) {
	assert(msg);
	assert(exchangeId);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	appStatus_AddExchangeId(msg, exchangeId);
}
/**
 * @brief Adds a duplicate of the tags JSON object to the status message. Deletes the old tags object if exists.
 * @param[in,out] msg: the status message
 * @param[in] tagsJsonHandle: the tags JSON object
 */
void AppStatus_CmdCtrl_AddTags(AppStatusMessage_T * msg, const cJSON * tagsJsonHandle) {
	assert(msg);
	assert(tagsJsonHandle);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	if(msg->tags) cJSON_Delete(msg->tags);
	msg->tags = cJSON_Duplicate((cJSON *)tagsJsonHandle, true);
}
/**
 * @brief Assigns the status code to the status message.
 * @param[in,out] msg: the status message
 * @param[in] statusCode: the new status code
 */
void AppStatus_CmdCtrl_AddStatusCode(AppStatusMessage_T * msg, const AppStatusMessage_StatusCode_T statusCode) {
	assert(msg);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	msg->statusCode = statusCode;
}
/**
 * @brief Assigns the descrCode to the status message.
 * @param[in,out] msg: the status message
 * @param[in] descrCode: the new description code
 */
void AppStatus_CmdCtrl_AddDescrCode(AppStatusMessage_T * msg, const AppStatusMessage_DescrCode_T descrCode) {
	assert(msg);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	msg->descrCode = descrCode;
}
/**
 * @brief Assigns the new details to the message. Deletes old details if exist.
 * @param[in,out] msg: the status message
 * @param[in] details: the new details string
 */
void AppStatus_CmdCtrl_AddDetails(AppStatusMessage_T * msg, const char * details) {
	assert(msg);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	assert(details);
	if(msg->details) free(msg->details);
	msg->details = copyString(details);
}
/**
 * @brief Adds a new item to the status message 'appError' containing the retcode as JSON. Only if retcode not RETCODE_OK.
 * @param[in,out] msg: the status message
 * @param[in] retcode: the retcode to add
 */
void AppStatus_CmdCtrl_AddRetcode(AppStatusMessage_T * msg, const Retcode_T retcode) {
	assert(msg);
	assert(msg->type == AppStatusMessage_Type_CmdCtrl);
	if(RETCODE_OK != retcode) AppStatus_AddStatusItem(msg, "appError", appStatus_GetRetcodeAsJson(retcode));
}
/**
 * @brief Adds a status item to the array of items under name of itemName to the status message.
 *
 * @param[in,out] msg: the status message
 * @param[in] itemName : the json object name of the item. Takes a copy.
 * @param[in] itemJsonHandle : the item json. Will NOT take a copy. Caller must not delete.
 */
void AppStatus_AddStatusItem(AppStatusMessage_T * msg, const char * itemName, cJSON * itemJsonHandle) {

	assert(msg);
	assert(itemName);
	assert(itemJsonHandle);

	cJSON * itemJsonObject = cJSON_CreateObject();

	cJSON_AddItemToObject(itemJsonObject, itemName, itemJsonHandle);

	if(msg->items == NULL) msg->items = cJSON_CreateArray();

	cJSON_AddItemToArray(msg->items, itemJsonObject);

}
/**
 * @brief Delete a status message. Deletes all parts (strings, json objects) of the message.
 * @param[in,out] msg: the message to delete
 */
static void appStatus_DeleteMessage(AppStatusMessage_T * msg) {

	assert(msg);

	if(NULL != msg->details) free(msg->details);

	if(NULL != msg->exchangeId) free(msg->exchangeId);

	if(NULL != msg->items) cJSON_Delete(msg->items);

	if(NULL != msg->tags) cJSON_Delete(msg->tags);

	free(msg);
}
/**
 * @brief Queue a json status message.
 * Do not use the jsonHandle afterwards, it is now owned by the queue.
 * Thread-safe.
 * @param[in] jsonHandle: the json status message
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_QUEUE_ERROR)
 */
static void appStatus_QueueJson4Sending(cJSON * jsonHandle) {

	if( pdFALSE == xSemaphoreTake(appStatus_JsonQueue_SemaphoreHandle, MILLISECONDS(APP_STATUS_JSON_QUEUE_SEMAPHORE_TAKE_ADD_WAIT_TICKS_MS))) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_QUEUE_ERROR));
	}

	if(appStatus_JsonQueue2Send_NextIndex == (APP_STATUS_QUEUE_TO_SEND_MAX-1) ) {
		appStatus_Stats_IncrementStatusSendFailedCounter();
		cJSON_Delete(jsonHandle);
	} else {
		// check if this pointer already exists
		// if it does, it's a code error
		bool duplicate = false;
		for(uint8_t i=0; i < appStatus_JsonQueue2Send_NextIndex; i++) {
			if(appStatus_JsonQueue2SendPtrArray[i] == jsonHandle) {
				duplicate = true;
				Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_STATUS_ATTEMPT_TO_QUEUE_DUPLICATE_MESSAGE));
			}
		}
		if(!duplicate) {
			appStatus_JsonQueue2SendPtrArray[appStatus_JsonQueue2Send_NextIndex] = jsonHandle;
			appStatus_JsonQueue2Send_NextIndex++;
		}
	}
	xSemaphoreGive(appStatus_JsonQueue_SemaphoreHandle);
}
/**
 * @brief Send all queued messages if module is enabled and connected to broker and resets the queue.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_STATUS_FAILED_TO_SEND_QUED_MSGS_NOT_ENABLED) if module is not enabled or connected to broker.
 */
static void appStatus_SendQueuedMessages(void) {

	if(!appStatus_isEnabled || !AppMqtt_IsConnected()) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_STATUS_FAILED_TO_SEND_QUED_MSGS_NOT_ENABLED));
		return;
	}

	// if can't get the semaphore now, queue will be sent at next opportunity
	if(pdTRUE == xSemaphoreTake(appStatus_JsonQueue_SemaphoreHandle, MILLISECONDS(APP_STATUS_JSON_QUEUE_SEMAPHORE_TAKE_SEND_WAIT_TICKS_MS))) {

		for(uint8_t i = 0; i < appStatus_JsonQueue2Send_NextIndex; i++) {

			cJSON * jsonHandle = appStatus_JsonQueue2SendPtrArray[i];

			appStatus_JsonQueue2SendPtrArray[i] = NULL;

			appStatus_SendJsonMessage(jsonHandle, true, false);

			vTaskDelay(APP_STATUS_QUEUE2SEND_WAIT_TICKS_MS);

		} // for loop

		appStatus_JsonQueue2Send_NextIndex = 0;

		xSemaphoreGive(appStatus_JsonQueue_SemaphoreHandle);
	}
}
/**
 * @brief Enqueues the sending of the status message in module's command processor.
 *
 * @param[in] msgPtr : the message. Will be deleted after it is sent.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE)
 */
void AppStatus_SendStatusMessage(AppStatusMessage_T * msgPtr)  {

	assert(msgPtr);

	Retcode_T retcode = CmdProcessor_Enqueue((CmdProcessor_T *) appStatus_ProcessorHandle, appStatus_SendStatusMessageEnqueue, msgPtr, UINT32_C(0));
	if(RETCODE_OK != retcode) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE));
	}
}
/**
 * @brief Create a new status message that is part of a sequence.
 * @param[in] descrCode: the description code
 * @param[in] details: details string, can be NULL
 * @param[in] exchangeId: exchange id string, can be NULL
 * @param[in] totalNumParts: the total number of messages in the sequence
 * @param[in] thisNumPart: the number of this message in the sequence
 * @return AppStatusMessage_T *: the newly created status message
 */
static AppStatusMessage_T * appStatus_CreateStatusMessagePart(const AppStatusMessage_DescrCode_T descrCode, const char * details, const char * exchangeId, uint8_t totalNumParts, uint8_t thisPartNum) {

	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Info, descrCode, details);

	if(exchangeId != NULL) appStatus_AddExchangeId(msg, exchangeId);

	appStatus_AddParts(msg, totalNumParts, thisPartNum);

	return msg;
}
/**
 * @brief Convenience function to create and send a status message as part of a series with optional Json item.
 * Enqueues the sending of the message to the internal command processor.
 *
 * @param[in] descrCode: the description code
 * @param[in] details: a details string, can be NULL
 * @param[in] exchangeId: if a response to a request, set the request exchangeId for service to correlate, NULL to leave out
 * @param[in] totalNumParts: the total number of message parts in the series
 * @param[in] thisPartNum: this part number in the series
 * @param[in] itemName: the json item name of the status item, can be NULL
 * @param[in] itemJsonHandle: the json of the item. function takes care of deleting it, do not delete after call. can be NULL
 *
 * @note If either of itemName or jsonHandle is NULL, no status item is set.
 *
 */
void AppStatus_SendStatusMessagePart(const AppStatusMessage_DescrCode_T descrCode, const char * details, const char * exchangeId, uint8_t totalNumParts, uint8_t thisPartNum, const char * itemName, cJSON * itemJsonHandle) {

	AppStatusMessage_T * msg = appStatus_CreateStatusMessagePart(descrCode, details, exchangeId, totalNumParts, thisPartNum);

	if(itemName!=NULL && itemJsonHandle!=NULL) AppStatus_AddStatusItem(msg, itemName, itemJsonHandle);
	if(itemName==NULL && itemJsonHandle!=NULL) cJSON_Delete((itemJsonHandle));

	AppStatus_SendStatusMessage(msg);
}
/**
 * @brief Sends the full status message as a series of messages. Used either in the periodic task or as a response to a command.
 * Will send queued messages first if descrCode is #AppStatusMessage_Descr_BootStatus.
 *
 * @param[in] exchangeIdStr: exchange Id, can be NULL
 * @param[in] descriptionCode: the description code
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref appStatus_SendJsonMessage()
 */
static Retcode_T appStatus_SendFullStatus(const char * exchangeIdStr, AppStatusMessage_DescrCode_T descrCode) {

	Retcode_T retcode = RETCODE_OK;

	bool sendQueuedMessagesFirst = (AppStatusMessage_Descr_BootStatus == descrCode) ? false : true;

	uint8_t totalNumParts = 6;

	AppStatusMessage_T * msg = NULL;

	// part 1
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, "GENERAL", exchangeIdStr, totalNumParts, 1);
		cJSON * statsJson = appStatus_Stats_GetAsJson();
		if(statsJson) AppStatus_AddStatusItem(msg, "stats", statsJson);
		else AppStatus_AddStatusItem(msg, "stats", cJSON_CreateNull());
		AppStatus_AddStatusItem(msg, "versions", AppMisc_GetVersionsAsJson());

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	// part 2
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, NULL, exchangeIdStr, totalNumParts, 2);
		AppStatus_AddStatusItem(msg, "activeRuntimeConfig.topicConfig", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_topicConfig));

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	// part 3
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, NULL, exchangeIdStr, totalNumParts, 3);
		AppStatus_AddStatusItem(msg, "activeRuntimeConfig.mqttBrokerConnectionConfig", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_mqttBrokerConnectionConfig));

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	// part 4
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, NULL, exchangeIdStr, totalNumParts, 4);
		AppStatus_AddStatusItem(msg, "activeRuntimeConfig.statusConfig", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_statusConfig));

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	// part 5
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, NULL, exchangeIdStr, totalNumParts, 5);
		AppStatus_AddStatusItem(msg, "activeRuntimeConfig.activeTelemetryRTParams", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_activeTelemetryRTParams));

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	// part 6
	if(RETCODE_OK == retcode) {
		msg = appStatus_CreateStatusMessagePart(descrCode, NULL, exchangeIdStr, totalNumParts, 6);
		AppStatus_AddStatusItem(msg, "activeRuntimeConfig.targetTelemetryConfig", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_targetTelemetryConfig));

		cJSON * jsonHandle = appStatus_GetStatusMessageAsJson(msg);
	    retcode = appStatus_SendJsonMessage(jsonHandle, false, sendQueuedMessagesFirst);
		appStatus_DeleteMessage(msg);
	}
	return retcode;
}
/**
 * @brief Wrapper around @ref appStatus_SendFullStatus() to send full status enqueued in module's command processor.
 * @param[in] exchangeIdStr: [const char * ] : exchange id, can be NULL
 * @param[in] descrCode: [AppStatusMessage_DescrCode_T] : the description code
 * @exception Retcode_RaiseError: retcode from @ref appStatus_SendFullStatus() if not RETCODE_OK
 */
static void appStatus_SendFullStatusEnqueue(void * exchangeIdStr, uint32_t descrCode) {

	Retcode_T retcode = appStatus_SendFullStatus((const char *) exchangeIdStr, (AppStatusMessage_DescrCode_T) descrCode);
	if(RETCODE_OK != retcode) Retcode_RaiseError(retcode);
}
/**
 * @brief Sends the short status message. Can be called enqueued or directly.
 * @param[in] exchangeIdStr: [const char *] the exchange id, can be NULL
 * @param[in] param2: unused
 */
static void appStatus_SendShortStatus(void * exchangeIdStr, uint32_t param2) {
	BCDS_UNUSED(param2);
	const char * exIdStr = (char * ) exchangeIdStr;

	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Info, AppStatusMessage_Descr_CurrentShortStatus, NULL);

	if(exIdStr!=NULL) appStatus_AddExchangeId(msg, exIdStr);

	cJSON * statsJson = appStatus_Stats_GetAsJson();
	if(statsJson) AppStatus_AddStatusItem(msg, "stats", statsJson);
	else AppStatus_AddStatusItem(msg, "stats", cJSON_CreateNull());

	AppStatus_AddStatusItem(msg, "activeRuntimeConfig.activeTelemetryRTParams", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_activeTelemetryRTParams));

	appStatus_SendStatusMessage(msg);

}
/**
 * @brief Send the boot status message series. Synchronous call, uses @ref appStatus_SendFullStatus().
 * Call from @ref AppController after boot / enable.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref appStatus_SendFullStatus()
 */
Retcode_T AppStatus_SendBootStatus(void) {

	Retcode_T retcode = RETCODE_OK;
	retcode = appStatus_SendFullStatus(NULL, AppStatusMessage_Descr_BootStatus);

	// remember (more or less) the last time we sent out the status
	appStatus_LastStatusSentTicks = xTaskGetTickCount();

	return retcode;
}
/**
 * @brief Send the full, current status of the device in multiple parts as a response to a command.
 * Enqueues the sending of the partial status messages into the AppStatus command processor. Function returns immediately.
 *
 * @param[in] exchangeIdStr: the exchange Id from the request.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE)
 */
void AppStatus_SendCurrentFullStatus(const char * exchangeIdStr) {

	assert(exchangeIdStr);

	Retcode_T retcode = CmdProcessor_Enqueue((CmdProcessor_T *)appStatus_ProcessorHandle, appStatus_SendFullStatusEnqueue, (char *)exchangeIdStr, AppStatusMessage_Descr_CurrentFullStatus);
	if(RETCODE_OK != retcode) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE));
	}
}
/**
 * @brief Send the short, current status of the device as a response to a command.
 * Enqueues the sending of the message into the module's command processor. Function returns immediately.
 *
 * @param[in] exchangeIdStr: the exchange Id from the request.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE)
 */
void AppStatus_SendCurrentShortStatus(const char * exchangeIdStr) {

	assert(exchangeIdStr);

	Retcode_T retcode = CmdProcessor_Enqueue((CmdProcessor_T *)appStatus_ProcessorHandle, appStatus_SendShortStatus, (char *)exchangeIdStr, 0);
	if(RETCODE_OK != retcode) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE));
	}
}
/**
 * @brief Send the currently active telemetry parameters as a response to a command.
 * Enqueues the sending of the message into the module's command processor, uses @ref AppStatus_SendStatusMessage(). Function returns immediately.
 *
 * @param[in] exchangeIdStr: the exchange Id from the request.
 * @exception Retcode_RaiseError: from @ref AppStatus_SendStatusMessage()
 */
void AppStatus_SendActiveTelemetryParams(const char * exchangeIdStr) {

	assert(exchangeIdStr);

	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Info, AppStatusMessage_Descr_ActiveTelemetryRuntimeParams, NULL);
	appStatus_AddExchangeId(msg, exchangeIdStr);
	AppStatus_AddStatusItem(msg, "activeRuntimeConfig.activeTelemetryRTParams", AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_activeTelemetryRTParams));
	AppStatus_SendStatusMessage(msg);
}
/**
 * @brief Send a status message that broker has been disconnected.
 * Enqueues the sending of the message into the module's command processor using @ref AppStatus_SendStatusMessage(). Function returns immediately.
 *
 * @note Since the broker is disconnected when sending this message, it will be stored in the internal queue of the module first.
 * The message will then be sent at re-connect time.
 *
 * @exception Retcode_RaiseError: from @ref AppStatus_SendStatusMessage()
 */
void AppStatus_SendMqttBrokerDisconnectedMessage(void) {

	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Warning, AppStatusMessage_Descr_MqttBrokerWasDisconnected, NULL);

	cJSON * statsJson = appStatus_Stats_GetAsJson();
	if(statsJson) AppStatus_AddStatusItem(msg, "stats", statsJson);
	else AppStatus_AddStatusItem(msg, "stats", cJSON_CreateNull());

	AppStatus_SendStatusMessage(msg);
}
/**
 * @brief Send a status message that the device was disconnected from the WLAN.
 * Enqueues the sending of the message into the module's command processor using @ref AppStatus_SendStatusMessage(). Function returns immediately.
 *
 * @note Since the broker is disconnected when sending this message, it will be stored in the internal queue of the module first.
 * The message will then be sent at re-connect time.
 *
 * @exception Retcode_RaiseError: from @ref AppStatus_SendStatusMessage()
 */
void AppStatus_SendWlanDisconnectedMessage(void) {
	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Warning, AppStatusMessage_Descr_WlanWasDisconnected, NULL);

	cJSON * statsJson = appStatus_Stats_GetAsJson();
	if(statsJson) AppStatus_AddStatusItem(msg, "stats", statsJson);
	else AppStatus_AddStatusItem(msg, "stats", cJSON_CreateNull());

	AppStatus_SendStatusMessage(msg);
}
/**
 * @brief Increment the 'disconnect counter' in the stats.
 */
void AppStatus_Stats_IncrementMqttBrokerDisconnectCounter(void) {
	appStatus_Stats_IncrementMqttBrokerDisconnectCounter();
}
/**
 * @brief Increment the 'wlan disconnect counter' in the stats.
 */
void AppStatus_Stats_IncrementWlanDisconnectCounter(void) {
	appStatus_Stats_IncrementWlanDisconnectCounter();
}
/**
 * @brief Increment the 'telemetry send failed' counter in the stats.
 */
void AppStatus_Stats_IncrementTelemetrySendFailedCounter(void) {
	appStatus_Stats_IncrementTelemetrySendFailedCounter();
}
/**
 * @brief Increment the 'telemetry too slow' counter in the stats.
 */
void AppStatus_Stats_IncrementTelemetrySendTooSlowCounter(void) {
	appStatus_Stats_IncrementTelemetrySendTooSlowCounter();
}
/**
 * @brief Increment the 'telemetry sampling failed' counter.
 */
void AppStatus_Stats_IncrementTelemetrySamplingTooSlowCounter(void) {
	appStatus_Stats_IncrementTelemetrySamplingTooSlowCounter();
}
/**
 * @brief Get the inernal stats as a JSON.
 * @return cJSON *: the json pointer or NULL if xSemaphoreTake timeout
 */
static cJSON * appStatus_Stats_GetAsJson(void) {

	if(pdTRUE == xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) )) {

		cJSON * jsonHandle = cJSON_CreateObject();

		assert(appStatus_Stats.bootTimestampStr);
		cJSON_AddItemToObject(jsonHandle, "bootTimestamp", cJSON_CreateString(appStatus_Stats.bootTimestampStr));

		cJSON_AddNumberToObject(jsonHandle,"bootBatteryVoltage", appStatus_Stats.bootBatteryVoltage);

		Retcode_T retcode = BatteryMonitor_MeasureSignal(&appStatus_Stats.currentBatteryVoltage);

		if(RETCODE_OK == retcode) cJSON_AddNumberToObject(jsonHandle,"currentBatteryVoltage", appStatus_Stats.currentBatteryVoltage);
		else Retcode_RaiseError(retcode);

		cJSON_AddNumberToObject(jsonHandle, "mqttBrokerDisconnectCounter", appStatus_Stats.mqttBrokerDisconnectCounter);

		cJSON_AddNumberToObject(jsonHandle, "wlanDisconnectCounter", appStatus_Stats.wlanDisconnectCounter);

		cJSON_AddNumberToObject(jsonHandle, "statusSendFailedCounter", appStatus_Stats.statusSendFailedCounter);

		cJSON_AddNumberToObject(jsonHandle, "telemetrySendFailedCounter", appStatus_Stats.telemetrySendFailedCounter);

		cJSON_AddNumberToObject(jsonHandle, "telemetrySendTooSlowCounter", appStatus_Stats.telemetrySendTooSlowCounter);

		cJSON_AddNumberToObject(jsonHandle, "telemetrySamplingTooSlowCounter", appStatus_Stats.telemetrySamplingTooSlowCounter);

		cJSON_AddNumberToObject(jsonHandle, "retcodeRaisedErrorCounter", appStatus_Stats.retcodeRaisedErrorCounter);

		xSemaphoreGive(appStatus_Stats_SemaphoreHandle);

		return jsonHandle;
	} else {
		return NULL;
	}
}
/**
 * @brief Increment the broker disconnect counter in the stats.
 */
static void appStatus_Stats_IncrementMqttBrokerDisconnectCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.mqttBrokerDisconnectCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the wlan disconnect counter in the stats.
 */
static void appStatus_Stats_IncrementWlanDisconnectCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.wlanDisconnectCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the status send failed counter in the stats.
 */
static void appStatus_Stats_IncrementStatusSendFailedCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.statusSendFailedCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the telemetry send failed counter in the stats.
 */
static void appStatus_Stats_IncrementTelemetrySendFailedCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.telemetrySendFailedCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the telemetry send too slow counter in the stats.
 */
static void appStatus_Stats_IncrementTelemetrySendTooSlowCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.telemetrySendTooSlowCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the telemetry sampling too slow counter in the stats.
 */
static void appStatus_Stats_IncrementTelemetrySamplingTooSlowCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.telemetrySamplingTooSlowCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Increment the retcode raised error counter in the stats.
 */
static void appStatus_Stats_IncrementRetcodeRaisedErrorCounter(void) {
	if(xSemaphoreTake(appStatus_Stats_SemaphoreHandle, MILLISECONDS(APP_STATUS_STATS_SEMAPHORE_TAKE_WAIT_MILLIS) ))
		appStatus_Stats.retcodeRaisedErrorCounter++;
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);
}
/**
 * @brief Create the recurring / periodic status send task.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_STATUS_FAILED_TO_CREATE_RECURRING_TASK)
 */
static void appStatus_CreateRecurringTask(void) {

	if(appStatus_TaskHandle != NULL) return;

	if (pdPASS != xTaskCreate(	appStatus_RecurringSendTask,
								(const char* const ) "StatusTask",
								APP_STATUS_RECURRING_TASK_STACK_SIZE,
								NULL,
								APP_STATUS_RECURRING_TASK_PRIOIRTY,
								&appStatus_TaskHandle)) {

		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_STATUS_FAILED_TO_CREATE_RECURRING_TASK));
	}
}
/**
 * @brief Create the recurring / periodic status send task if configured.
 * @return Retcode_T: RETCODE_OK
 * @exception Retcode_RaiseError: @ref appStatus_CreateRecurringTask().
 */
Retcode_T AppStatus_CreateRecurringTask(void) {

	Retcode_T retcode = RETCODE_OK;

	if(appStatus_isPeriodicStatus) appStatus_CreateRecurringTask();

	return retcode;
}
/**
 * @brief Delete the recurring send task if it is running.
 *
 * @return Retcode_T: RETCODE_OK
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_STATUS_FAILED_TO_DELETE_RECURRING_TASK)
 */
Retcode_T AppStatus_SuspendRecurringTask(void) {

	Retcode_T retcode = RETCODE_OK;

	if(appStatus_TaskHandle==NULL) return RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appStatus_TaskSemaphoreHandle, appStatus_PeriodicStatusIntervalMillis * 4)) {
		vTaskDelete(appStatus_TaskHandle);
		xSemaphoreGive(appStatus_TaskSemaphoreHandle);
	} else {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_STATUS_FAILED_TO_DELETE_RECURRING_TASK));
	}
	appStatus_TaskHandle = NULL;

	return retcode;
}
/**
 * @brief Send all queued messages if the module is enabled and the broker connected. This is a synchronous call.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_STATUS_FAILED_TO_SEND_QUED_MSGS_NOT_ENABLED)
 */
void AppStatus_SendQueuedMessages(void) {

	if(!appStatus_isEnabled || !AppMqtt_IsConnected()) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_STATUS_FAILED_TO_SEND_QUED_MSGS_NOT_ENABLED));
		return;
	}

	appStatus_SendQueuedMessages();
}
/**
 * @brief The recurring send task. Runs it's own loop, calculates the time to sleep based on the periodicity configured.
 * Sends either @ref appStatus_SendFullStatus() or @ref appStatus_SendShortStatus() depending on configuration.
 */
static void appStatus_RecurringSendTask(void* pvParameters) {
	BCDS_UNUSED(pvParameters);

	// calculate when to start again
	// take into account this could have been suspended
	// note: does not hit the exact millisecond from before
	uint32_t ticksSinceLastSent = appStatus_LastStatusSentTicks + xTaskGetTickCount();
	uint32_t nextSendTicks = ticksSinceLastSent + appStatus_PeriodicStatusIntervalMillis;
	if(ticksSinceLastSent < nextSendTicks) {
		uint32_t waitTicks = nextSendTicks - ticksSinceLastSent;
		vTaskDelay(waitTicks);
	}

    TickType_t startLoopTicks = 0;
    int32_t loopDelayTicks = 0;

    while (1) {

		startLoopTicks = xTaskGetTickCount();

    	if(pdTRUE == xSemaphoreTake(appStatus_TaskSemaphoreHandle, appStatus_PeriodicStatusIntervalMillis * 2)) {

    		if(AppRuntimeConfig_PeriodicStatusType_Full == appStatus_PeriodicStatusType) {

				appStatus_SendFullStatus(NULL, AppStatusMessage_Descr_CurrentFullStatus);

    		} else if(AppRuntimeConfig_PeriodicStatusType_Short == appStatus_PeriodicStatusType) {

				appStatus_SendShortStatus(NULL, 0);

    		} else assert(0);

    		appStatus_LastStatusSentTicks = xTaskGetTickCount();

    		xSemaphoreGive(appStatus_TaskSemaphoreHandle);
    	}

		//calculate delay and wait
		loopDelayTicks = appStatus_PeriodicStatusIntervalMillis-(xTaskGetTickCount()-startLoopTicks)-1;
		if(loopDelayTicks > 0) vTaskDelay((TickType_t) loopDelayTicks);
    }
}
/**
 * @brief Get the package Id description from the package id to construct the Retcode JSON.
 * @param[in] packageId: the package id
 * @return char *: pointer to static package id string.
 */
static char * appStatus_getPackageIdStr(uint32_t packageId) {

    char * packageStr = "Unclassified";

	switch(packageId) {

	case SOLACE_APP_PACKAGE_ID:
		packageStr = SOLACE_APP_PACKAGE_DESCR;
		break;

	// this is a copy from SystemStartup.c - DefaultErrorHandlingFunc
	case 2:
		packageStr = "Utils";
		break;
	case 6:
		packageStr = "Algo";
		break;
	case 7:
		packageStr = "SensorUtils";
		break;
	case 8:
		packageStr = "Sensors";
		break;
	case 9:
		packageStr = "SensorToolbox";
		break;
	case 10:
		packageStr = "WLAN";
		break;
	case 13:
		packageStr = "BLE";
		break;
	case 26:
		packageStr = "FOTA";
		break;
	case 30:
		packageStr = "ServalPAL";
		break;
	case 33:
		packageStr = "LoRaDrivers";
		break;
	case 35:
		packageStr = "Essentials";
		break;
	case 36:
		packageStr = "Drivers";
		break;
	case 101:
		packageStr = "BSTLib";
		break;
	case 103:
		packageStr = "FreeRTOS";
		break;
	case 105:
		packageStr = "BLEStack";
		break;
	case 106:
		packageStr = "EMlib";
		break;
	case 107:
		packageStr = "ServalStack";
		break;
	case 108:
		packageStr = "WiFi";
		break;
	case 112:
		packageStr = "FATfs";
		break;
	case 115:
		packageStr = "Escrypt_CycurTLS";
		break;
	case 122:
		packageStr = "GridEye";
		break;
	case 123:
		packageStr = "BSX";
		break;
	case 153:
		packageStr = "XDK110 Application";
		break;
	case 175:
		packageStr = "BSP";
		break;
	default:
		break;
	}

	return packageStr;
}
/**
 * @brief Get the severity description from severity.
 * @param[in] severity: the retcode severity
 * @return char *: pointer to the static severity string
 */
static char * appStatus_getSeverityStr(Retcode_Severity_T severity) {
	char * severityStr = "RETCODE_SEVERITY_UNKNOWN";
	switch (severity) {
	case RETCODE_SEVERITY_FATAL:
		severityStr = "RETCODE_SEVERITY_FATAL";
		break;
	case RETCODE_SEVERITY_ERROR:
		severityStr = "RETCODE_SEVERITY_ERROR";
		break;
	case RETCODE_SEVERITY_WARNING:
		severityStr = "RETCODE_SEVERITY_WARNING";
		break;
	case RETCODE_SEVERITY_INFO:
		severityStr = "RETCODE_SEVERITY_INFO";
		break;
	default:
	  break;
	}
	return severityStr;
}
/**
 * @brief Get the module description from the module id.
 * @param[in] moduleId: the module id
 * @return char *: pointer to the static module id string
 */
static char * appStatus_getModuleIdStr(uint32_t moduleId) {
	char * moduleIdStr = "XDK_MODULE";
	switch(moduleId) {
	case SOLACE_APP_MODULE_ID_MAIN:
		moduleIdStr = "SOLACE_APP_MODULE_ID_MAIN";
		break;
	case SOLACE_APP_MODULE_ID_APP_CONTROLLER:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_CONTROLLER";
		break;
	case SOLACE_APP_MODULE_ID_APP_MQTT:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_MQTT";
		break;
	case SOLACE_APP_MODULE_ID_APP_XDK_MQTT:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_XDK_MQTT";
		break;
	case SOLACE_APP_MODULE_ID_APP_CMD_CTRL:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_CMD_CTRL";
		break;
	case SOLACE_APP_MODULE_ID_APP_CONFIG:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_CONFIG";
		break;
	case SOLACE_APP_MODULE_ID_APP_MISC:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_MISC";
		break;
	case SOLACE_APP_MODULE_ID_APP_RUNTIME_CONFIG:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_RUNTIME_CONFIG";
		break;
	case SOLACE_APP_MODULE_ID_APP_TELEMETRY_PAYLOAD:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_TELEMETRY_PAYLOAD";
		break;
	case SOLACE_APP_MODULE_ID_APP_TELEMETRY_PUBLISH:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_TELEMETRY_PUBLISH";
		break;
	case SOLACE_APP_MODULE_ID_APP_TELEMETRY_QUEUE:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_TELEMETRY_QUEUE";
		break;
	case SOLACE_APP_MODULE_ID_APP_TELEMETRY_SAMPLING:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_TELEMETRY_SAMPLING";
		break;
	case SOLACE_APP_MODULE_ID_APP_BUTTONS:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_BUTTONS";
		break;
	case SOLACE_APP_MODULE_ID_APP_STATUS:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_STATUS";
		break;
	case SOLACE_APP_MODULE_ID_APP_VERSION:
		moduleIdStr = "SOLACE_APP_MODULE_ID_APP_VERSION";
		break;
	default: break;
	}
	return moduleIdStr;
}
/**
 * @brief Get the retcode as a JSON object.
 * @param[in] retcode: the retcode
 * @return cJSON *: the newly created JSON object
 */
static cJSON * appStatus_GetRetcodeAsJson(const Retcode_T retcode) {

	uint32_t packageId = Retcode_GetPackage(retcode);
	char * packageStr = appStatus_getPackageIdStr(packageId);

	uint32_t code = Retcode_GetCode(retcode);

	uint32_t moduleId = Retcode_GetModuleId(retcode);
	char * moduleIdStr = appStatus_getModuleIdStr(moduleId);

	Retcode_Severity_T severity = Retcode_GetSeverity(retcode);
	char * severityStr = appStatus_getSeverityStr(severity);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON_AddItemToObject(jsonHandle, "tickCountRaised", cJSON_CreateNumber(xTaskGetTickCount()));
	cJSON_AddItemToObject(jsonHandle, "severity", cJSON_CreateString(severityStr));
	cJSON_AddItemToObject(jsonHandle, "package", cJSON_CreateString(packageStr));
	cJSON_AddItemToObject(jsonHandle, "module", cJSON_CreateString(moduleIdStr));
	cJSON_AddItemToObject(jsonHandle, "packageId", cJSON_CreateNumber(packageId));
	cJSON_AddItemToObject(jsonHandle, "moduleId", cJSON_CreateNumber(moduleId));
	cJSON_AddItemToObject(jsonHandle, "severityId", cJSON_CreateNumber(severity));
	cJSON_AddItemToObject(jsonHandle, "code", cJSON_CreateNumber(code));

	return jsonHandle;

}
/**
 * @brief Initialize the error handling function.
 * Call first in @ref main().
 *
 * **Example Usage**
 * @code
 * int main(void) {
 * 		Retcode_T retcode = RETCODE_OK;
 * 		if (RETCODE_OK == retcode) retcode = AppStatus_InitErrorHandling();
 * 		if (RETCODE_OK == retcode) retcode = Retcode_Initialize(AppStatus_ErrorHandlingFunc);
 *
 * 		...
 * }
 * @endcode
 *
 * @see AppStatus_ErrorHandlingFunc()
 */
Retcode_T AppStatus_InitErrorHandling(void) {

	Retcode_T retcode = RETCODE_OK;

	appStatus_ErrorHandlingFunc_SemaphoreHandle = xSemaphoreCreateBinary();
	if(appStatus_ErrorHandlingFunc_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appStatus_ErrorHandlingFunc_SemaphoreHandle);

	appStatus_Stats_SemaphoreHandle = xSemaphoreCreateBinary();
	if(appStatus_Stats_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appStatus_Stats_SemaphoreHandle);

	return retcode;
}

/**
 * @brief Custom error handling function. Initialize in @ref main().
 * Prints a json format of the retcode.
 * Sends a status message for severity RETCODE_SEVERITY_ERROR and RETCODE_SEVERITY_FATAL.
 * Reboots for severy RETCODE_SEVERITY_FATAL.
 * Thread-safety: semaphore take timeout is the longest defined publishing timeout plus overhead.
 *
 * @note Uses a copy of the package id to string mapping, hence needs changing here if XDK packages change.
 *
 * @see AppStatus_InitErrorHandling()
 *
 * **Example Usage**
 * @code
 *
 * retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_PUBLSIH_PAYLOAD_GT_MAX_PUBLISH_DATA_LENGTH);
 *
 * // if you want the error to be printed and sent
 *
 * Retcode_RaiseError(retcode);
 *
 * or
 *
 * retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_MQTT_PUBLSIH_PAYLOAD_GT_MAX_PUBLISH_DATA_LENGTH);
 *
 * // if you want the error to be printed, sent and the XDK rebooted
 *
 * Retcode_RaiseError(retcode);
 *
 * @endcode
 */
void AppStatus_ErrorHandlingFunc(Retcode_T retcode, bool isfromIsr) {

	// cannot handle it
    if (isfromIsr) return;

	if(pdTRUE == xSemaphoreTake(appStatus_ErrorHandlingFunc_SemaphoreHandle, MILLISECONDS(APP_STATUS_ERROR_HANDLING_FUNC_SEMAPHORE_TAKE_WAIT_IN_MS))) {

		cJSON * jsonHandle = appStatus_GetRetcodeAsJson(retcode);

		printf("[INFO] - AppStatus_ErrorHandlingFunc: raised error:\r\n");
		printJSON(jsonHandle);

		Retcode_Severity_T severity = Retcode_GetSeverity(retcode);
		if(RETCODE_SEVERITY_ERROR == severity || RETCODE_SEVERITY_FATAL == severity) {

			appStatus_Stats_IncrementRetcodeRaisedErrorCounter();

			AppStatusMessage_T * msgPtr = AppStatus_CreateMessage(AppStatusMessage_Status_Error, AppStatusMessage_Descr_InternalAppError, NULL);
			AppStatus_AddStatusItem(msgPtr, "appError", jsonHandle);
			AppStatus_SendStatusMessage(msgPtr);

		} else {
			cJSON_Delete(jsonHandle);
		}

		if(RETCODE_SEVERITY_FATAL == severity) {
			printf("[FATAL-ERROR] - rebooting in 5 seconds ...\r\n");
			// note: fatal should really be synchronous and not let the raiser continue.
			// waiting for 5 seconds to achieves this, probably works most times ...
			vTaskDelay(5000);
			BSP_Board_SoftReset();
		}

		xSemaphoreGive(appStatus_ErrorHandlingFunc_SemaphoreHandle);

	} else assert(0);
}

/**@} */
/** ************************************************************************* */



