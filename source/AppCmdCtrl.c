/*
 * AppCmdCtrl.c
 *
 *  Created on: 23 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppCmdCtrl AppCmdCtrl
 * @{
 *
 * @brief Manages the command & control API
 *
 * @author $(SOLACE_APP_AUTHOR)
 *
 * @date $(SOLACE_APP_DATE)
 *
 * @file
 *
 **/
#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID /**< undefine any previous module id */
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_CMD_CTRL

#include "AppCmdCtrl.h"
#include "AppMisc.h"
#include "AppStatus.h"

#include "cJSON.h"
#include "BCDS_Assert.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "BCDS_BSP_Board.h"

/**
 * @brief Enum to capture the action after receiving a command/config message.
 */
enum AppCmdCtrl_ProcessType_E {
	AppCmdCtrl_ProcessType_Process = 0,
	AppCmdCtrl_ProcessType_RejectBusyWithReply
};
typedef enum AppCmdCtrl_ProcessType_E AppCmdCtrl_ProcessType_T; /**< type for #AppCmdCtrl_ProcessType_E */

/* commands */
#define COMMAND_SUSPEND_TELEMETRY							"SUSPEND_TELEMETRY" /**< COMMAND_SUSPEND_TELEMETRY*/
#define COMMAND_RESUME_TELEMETRY							"RESUME_TELEMETRY"  /**< COMMAND_RESUME_TELEMETRY*/
#define COMMAND_SEND_FULL_STATUS							"SEND_FULL_STATUS"  /**< COMMAND_SEND_FULL_STATUS*/
#define COMMAND_SEND_SHORT_STATUS							"SEND_SHORT_STATUS"  /**< COMMAND_SEND_SHORT_STATUS*/
#define COMMAND_SEND_ACTIVE_TELEMETRY_PARAMS				"SEND_ACTIVE_TELEMETRY_PARAMS"  /**< COMMAND_SEND_ACTIVE_TELEMETRY_PARAMS*/
#define COMMAND_REBOOT										"REBOOT"  /**< COMMAND_REBOOT*/
#define COMMAND_SEND_ACTIVE_RUNTIME_CONFIG					"SEND_ACTIVE_RUNTIME_CONFIG"  /**< COMMAND_SEND_ACTIVE_RUNTIME_CONFIG*/
#define COMMAND_SEND_RUNTIME_CONFIG_FILE					"SEND_RUNTIME_CONFIG_FILE"  /**< COMMAND_SEND_RUNTIME_CONFIG_FILE*/
#define COMMAND_DELETE_RUNTIME_CONFIG_FILE					"DELETE_RUNTIME_CONFIG_FILE"  /**< COMMAND_DELETE_RUNTIME_CONFIG_FILE*/
#define COMMAND_PERSIST_ACTIVE_CONFIG						"PERSIST_ACTIVE_CONFIG"  /**< COMMAND_PERSIST_ACTIVE_CONFIG*/
#define COMMAND_TRIGGER_SAMPLE_ERROR						"TRIGGER_SAMPLE_ERROR" /**< COMMAND_TRIGGER_SAMPLE_ERROR*/
#define COMMAND_TRIGGER_SAMPLE_FATAL_ERROR					"TRIGGER_SAMPLE_FATAL_ERROR" /**< COMMAND_TRIGGER_SAMPLE_FATAL_ERROR*/
#define COMMAND_SEND_VERSION_INFO							"SEND_VERSION_INFO" /**< COMMAND_SEND_VERSION_INFO */

#define APP_CMD_CTRL_WAIT_BETWEEN_SUBSCRIPTION_REQUESTS_IN_MS			(UINT32_C(1000)) /**< wait time between subscription requests */

static CmdProcessor_T * appCmdCtrl_ProcessorHandle = NULL; /**< processor handle for the module */

static SemaphoreHandle_t appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle = NULL; /**< internal semaphore to serialize instructions */
#define APP_CMD_CTRL_INSTRUCTION_PROCESSING_IN_PROGRESS_SEMAPHORE_WAIT_IN_MS		UINT32_C(10) /**< wait in millis for a new instruction to be processed */
#define APP_CMD_CTRL_BLOCK_INSTRUCTION_PROCESSING_SEMAPHORE_WAIT_IN_MS				UINT32_C(2000) /**< wait time for blocking new instruction processing */

/* local copies of configuration */
static char * appCmdCtrl_DeviceId = NULL; /**< local copy of the device id */
static bool appCmdCtrl_isCleanSession = true; /**< flag to capture if broker connection is a clean session or not. used to determine if subscriptions need to be sent on re-connect */

static AppControllerApplyRuntimeConfiguration_Func_T appCmdCtrl_AppControllerApplyRuntimeConfigurationFunc = NULL; /**< callback function for new configuration instruction */
static AppControllerExecuteCommand_Func_T appCmdCtrl_AppControllerExecuteCommandFunc = NULL; /**< callback function for new command instruction */

/* forwards */
static void appCmdCtrl_SubscriptionCallBack(AppXDK_MQTT_IncomingDataCallbackParam_T * params);

static Retcode_T appCmdCtrl_PubSubSetup(const AppRuntimeConfig_TopicConfig_T * configPtr);

static Retcode_T appCmdCtrl_DeleteSubscriptions(void);


#define APP_CMD_CTRL_NUM_BASE_TOPIC_LEVELS		UINT8_C(3) /**< number of levels in the base topic / resource categorization */
/**
 * @brief Index to iterate over topic levels.
 */
enum AppCmdCtrl_TopicType_E {
	AppCmdCtrl_TopicType_SingleDevice = 0,
	AppCmdCtrl_TopicType_Level3,
	AppCmdCtrl_TopicType_Level2,
	AppCmdCtrl_TopicType_Level1,
	AppCmdCtrl_TopicType_Max
};
typedef enum AppCmdCtrl_TopicType_E AppCmdCtrl_TopicType_T; /**< type for #AppCmdCtrl_TopicType_E */

static AppXDK_MQTT_Subscribe_T appCmdCtrl_SubscribeInfosCommandsArray[AppCmdCtrl_TopicType_Max]; /**< subscriptions for command instructions */
static AppXDK_MQTT_Subscribe_T appCmdCtrl_SubscribeInfosConfigurationsArray[AppCmdCtrl_TopicType_Max]; /**< subscriptions for configuration instructions */
/**
 * @brief Template for creating subscriptions.
 */
static AppXDK_MQTT_Subscribe_T appCmdCtrl_SubscribeInfoTemplate = {
	.topic = NULL,
	.qos = 0UL,
};

/* internal state management */
/**
 * @brief Internal notification that instruction processing has finished. Gives the semaphore.
 */
static void appCmdCtrl_NotifyInstructionProcessingFinished(void) {
	xSemaphoreGive(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle);
}
/**
 * @brief Internal function to block instruction processing. Takes the semaphore.
 * @return bool: true if semaphore was free, false if it was already taken.
 */
static bool appCmdCtrl_BlockInstructionProcessing(void) {
	return xSemaphoreTake(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle, MILLISECONDS(APP_CMD_CTRL_BLOCK_INSTRUCTION_PROCESSING_SEMAPHORE_WAIT_IN_MS));
}
/**
 * @brief Internal notification that instruction processing is allowed again. Gives the semaphore.
 */
static void appCmdCtrl_AllowInstructionProcessing(void) {
	xSemaphoreGive(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle);
}
/**
 * @brief Check if instruction processing is allowed.
 * @return bool
 */
static bool appCmdCtrl_IsInstructionProcessingAllowed(void) {
	if(pdTRUE == xSemaphoreTake(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle, MILLISECONDS(0))) {
		xSemaphoreGive(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle);
		return true;
	} else return false;
}

/**
 * @brief Calls the AppController callback to process a configuration instruction.
 * @param[in] configElement: the type of configuration instruction
 * @param[in] newConfigPtr: the new configuration
 * @param[in] delay2ApplyTicks: ticks to wait before calling the AppController callback
 * @return Retcode_T: the retcode from AppController callback function
 */
static Retcode_T applyNewRuntimeConfiguration(AppRuntimeConfig_ConfigElement_T configElement, void * newConfigPtr, uint32_t delay2ApplyTicks) {

    assert(newConfigPtr);

	vTaskDelay(delay2ApplyTicks);

    Retcode_T retcode = appCmdCtrl_AppControllerApplyRuntimeConfigurationFunc(configElement, newConfigPtr);

    return retcode;
}
/**
 * @brief Calls the AppController callback to process a command instruction.
 * @param[in] commandType: the command
 * @param[in] delay2ApplyTicks: ticks to wait before calling the AppController callback
 * @param[in] exchangeIdStr: the exchangeId set by the calling application. All responses should include the exchangeId.
 * @return Retcode_T: the retcode from AppController callback function
 */
static Retcode_T appCmdCtrl_executeCommand(const AppCmdCtrl_CommandType_T commandType, const uint32_t delay2ApplyTicks, const char * exchangeIdStr) {

	assert(exchangeIdStr);

	vTaskDelay(delay2ApplyTicks);

	Retcode_T retcode = appCmdCtrl_AppControllerExecuteCommandFunc(commandType, exchangeIdStr);

	return retcode;

}
/**
 * @brief Returns the callback for incoming subscription data.
 * @return AppXDK_MQTT_IncomingDataCallback_Func_T : the callback
 */
AppXDK_MQTT_IncomingDataCallback_Func_T AppCmdCtrl_GetGlobalSubscriptionCallback(void) {
	return appCmdCtrl_SubscriptionCallBack;
}

/**
 *  @brief Initialize the module.
 *
 *  @param[in] deviceId: the device id
 *  @param[in] isCleanSession: flag if connection to broker uses clean session. if yes, then each time on reconnect, the subscriptions are sent to broker again.
 *  @param[in] processorHandle: the processor for this module
 *  @param[in] appControllerApplyRuntimeConfigurationFunc: the callback for new configuration instructions
 *  @param[in] appControllerExecuteCommandFunc: the callback for new command instructions
 *
 *  @return     RETCODE_OK
 *  @return     RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_CMD_PROCESSOR_IS_NULL)
 *  @return     RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FUNCTION_IS_NULL)
 */
Retcode_T AppCmdCtrl_Init(	const char * deviceId,
							bool isCleanSession,
							const CmdProcessor_T * processorHandle,
							AppControllerApplyRuntimeConfiguration_Func_T appControllerApplyRuntimeConfigurationFunc,
							AppControllerExecuteCommand_Func_T appControllerExecuteCommandFunc) {

	assert(deviceId);
	assert(processorHandle);
	assert(appControllerApplyRuntimeConfigurationFunc);
	assert(appControllerExecuteCommandFunc);

	Retcode_T retcode = RETCODE_OK;

	if(processorHandle != NULL) appCmdCtrl_ProcessorHandle = (CmdProcessor_T *) processorHandle;
	else return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_CMD_PROCESSOR_IS_NULL);

	appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle = xSemaphoreCreateBinary();
	if(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle);

	if(!appControllerExecuteCommandFunc || !appControllerApplyRuntimeConfigurationFunc) {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FUNCTION_IS_NULL);
	} else {
		appCmdCtrl_AppControllerApplyRuntimeConfigurationFunc = appControllerApplyRuntimeConfigurationFunc;
		appCmdCtrl_AppControllerExecuteCommandFunc = appControllerExecuteCommandFunc;
	}

	appCmdCtrl_DeviceId = copyString(deviceId);
	appCmdCtrl_isCleanSession = isCleanSession;

	// initialize the topic arrays
	for(int i=0; i < AppCmdCtrl_TopicType_Max; i++) {
		appCmdCtrl_SubscribeInfosCommandsArray[i] = appCmdCtrl_SubscribeInfoTemplate;
		appCmdCtrl_SubscribeInfosConfigurationsArray[i] = appCmdCtrl_SubscribeInfoTemplate;
	}

	return retcode;
}
/**
 * @brief Setup of the module. Does nothing.
 * @return Retcode_T : RETCODE_OK
 */
Retcode_T AppCmdCtrl_Setup(void) {

	Retcode_T retcode = RETCODE_OK;

	return retcode;
}
/**
 * @brief Enable the module.
 * @details Sends the subscriptions to the various command and configuration topics with a wait in between.
 * @param[in] configPtr : the fully validated configuration
 * @return Retcode_T : RETCODE_OK or retcode from @ref appCmdCtrl_PubSubSetup().
 */
Retcode_T AppCmdCtrl_Enable(const AppRuntimeConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->topicConfigPtr);

	Retcode_T retcode = RETCODE_OK;

	retcode = appCmdCtrl_PubSubSetup(configPtr->topicConfigPtr);

	return retcode;
}
/**
 * @brief Apply a new runtime configuration.
 * @note For @ref AppRuntimeConfig_ConfigElement_T .AppRuntimeConfig_Element_topicConfig: sends a delete subscriptions and sends new subscriptions.
 * @note Does not check if the topic configuration has actually changed.
 *
 * @warning Do not call while instruction processing is allowed.
 *
 * @param[in] configElement : the configuration element, only @ref AppRuntimeConfig_Element_topicConfig supported
 * @param[in] configPtr : the configuration pointer as per configElement
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: from @ref appCmdCtrl_DeleteSubscriptions(), @ref appCmdCtrl_PubSubSetup()
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 *
 */
Retcode_T AppCmdCtrl_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, void * configPtr) {
	Retcode_T retcode = RETCODE_OK;

	assert(!appCmdCtrl_IsInstructionProcessingAllowed());

	if(AppRuntimeConfig_Element_topicConfig == configElement) {

		if(RETCODE_OK == retcode) retcode = appCmdCtrl_DeleteSubscriptions();

		if(RETCODE_OK == retcode) retcode = appCmdCtrl_PubSubSetup((AppRuntimeConfig_TopicConfig_T *)configPtr);

	} else retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);

	return retcode;
}
/**
 * @brief Notification of a re-connect. If cleanSession flag is set to true, sends subscriptions again to the broker.
 * Allows instruction processing after finished.
 * Instruction processing is blocked by the call to @ref AppCmdCtrl_NotifyDisconnectedFromBroker().
 *
 * @note If cleanSession flag is 'false' and the MQTT session was deleted on the broker, no subscriptions will exist after a re-connect.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref appCmdCtrl_PubSubSetup()
 */
Retcode_T AppCmdCtrl_NotifyReconnected2Broker(void) {

	Retcode_T retcode = RETCODE_OK;

	if(appCmdCtrl_isCleanSession) {
		retcode = appCmdCtrl_PubSubSetup(getAppRuntimeConfigPtr()->topicConfigPtr);
	}

	appCmdCtrl_AllowInstructionProcessing();

	return retcode;
}
/**
* @brief Notification of a disconnect. Blocks instruction processing for the module. Acts in concert with @ref AppCmdCtrl_NotifyReconnected2Broker().
* @return Retcode_T : RETCODE_OK
*/
Retcode_T AppCmdCtrl_NotifyDisconnectedFromBroker(void) {
	appCmdCtrl_BlockInstructionProcessing();
	return RETCODE_OK;
}
/**
 * @brief Sets up the subscriptions based on the topic configuration.
 * @param[in] configPtr: the topic configuration
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref AppMqtt_Subscribe()
 *
 * @details The following subscription topics are constructed using @ref AppMisc_FormatTopic():
 *
 * @details Single device command and configuration topic:
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/%s/command", methodCreate, baseTopic, appCmdCtrl_DeviceId) @endcode
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/%s/configuration", methodUpdate, baseTopic, appCmdCtrl_DeviceId) @endcode
 *
 * @details All devices in the same 3 level resource categorization:
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/command", methodCreate, baseTopic, NULL) @endcode
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/configuration", methodUpdate, baseTopic, NULL) @endcode
 *
 * @details All devices in the same 2 level resource categorization:
 * @code AppMisc_FormatTopic("%s/iot-control/%s/%s/device/command", methodCreate, baseTopicLevelsArray[0], baseTopicLevelsArray[1]) @endcode
 * @code AppMisc_FormatTopic("%s/iot-control/%s/%s/device/configuration", methodUpdate, baseTopicLevelsArray[0], baseTopicLevelsArray[1]) @endcode
 *
 * @details All devices in the same 1 level resource categorization:
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/command", methodCreate, baseTopicLevelsArray[0], NULL) @endcode
 * @code AppMisc_FormatTopic("%s/iot-control/%s/device/configuration", methodUpdate, baseTopicLevelsArray[0], NULL) @endcode
 */
static Retcode_T appCmdCtrl_PubSubSetup(const AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->received.methodCreate);
	assert(configPtr->received.methodUpdate);
	assert(configPtr->received.baseTopic);
	assert(appCmdCtrl_DeviceId);

	// set up all the topics
	char * methodUpdate = configPtr->received.methodUpdate;
	char * methodCreate = configPtr->received.methodCreate;
	char * baseTopic = configPtr->received.baseTopic;
	char * baseTopicLevelsArray[APP_CMD_CTRL_NUM_BASE_TOPIC_LEVELS];

	static char copyOfBaseTopic[255];
	memset(copyOfBaseTopic, '\0', 255);
	memcpy(copyOfBaseTopic, baseTopic,strlen(baseTopic));

	char * token = strtok(copyOfBaseTopic, "/");
	int index = 0;
	while(token != NULL) {
		assert(index < APP_CMD_CTRL_NUM_BASE_TOPIC_LEVELS);
		baseTopicLevelsArray[index] = copyString(token);
		token = strtok(NULL, "/");
		index++;
	}

	// set-up subscribe topics
	for(int i=AppCmdCtrl_TopicType_SingleDevice; i < AppCmdCtrl_TopicType_Max; i++) {

		// free all the existing topic strings
		if(appCmdCtrl_SubscribeInfosCommandsArray[i].topic) free(appCmdCtrl_SubscribeInfosCommandsArray[i].topic);
		if(appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic) free(appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic);

		// set the template
		appCmdCtrl_SubscribeInfosCommandsArray[i] = appCmdCtrl_SubscribeInfoTemplate;
		appCmdCtrl_SubscribeInfosConfigurationsArray[i] = appCmdCtrl_SubscribeInfoTemplate;

		switch(i) {
			case AppCmdCtrl_TopicType_SingleDevice: {

				appCmdCtrl_SubscribeInfosCommandsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/%s/command", methodCreate, baseTopic, appCmdCtrl_DeviceId);

				appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/%s/configuration", methodUpdate, baseTopic, appCmdCtrl_DeviceId);

			} break;
			case AppCmdCtrl_TopicType_Level3: {

				appCmdCtrl_SubscribeInfosCommandsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/command", methodCreate, baseTopic, NULL);

				appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/configuration", methodUpdate, baseTopic, NULL);

			} break;
			case AppCmdCtrl_TopicType_Level2: {

				appCmdCtrl_SubscribeInfosCommandsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/%s/device/command", methodCreate, baseTopicLevelsArray[0], baseTopicLevelsArray[1]);

				appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/%s/device/configuration", methodUpdate, baseTopicLevelsArray[0], baseTopicLevelsArray[1]);

			} break;
			case AppCmdCtrl_TopicType_Level1: {

				appCmdCtrl_SubscribeInfosCommandsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/command", methodCreate, baseTopicLevelsArray[0], NULL);

				appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic = AppMisc_FormatTopic("%s/iot-control/%s/device/configuration", methodUpdate, baseTopicLevelsArray[0], NULL);

			} break;
			default: assert(0);
		}
	} // for

	for(int i=0; i < APP_CMD_CTRL_NUM_BASE_TOPIC_LEVELS; i++) if(baseTopicLevelsArray[i]) free(baseTopicLevelsArray[i]);

	// now subscribe to new topics

	Retcode_T retcode = RETCODE_OK;

	for(int i=0; i < AppCmdCtrl_TopicType_Max; i++) {

		// command subscribe
		if(RETCODE_OK == retcode) retcode = AppMqtt_Subscribe(&(appCmdCtrl_SubscribeInfosCommandsArray[i]));

		// better chance to avoid disconnects while subscribing
		if(RETCODE_OK == retcode) vTaskDelay(MILLISECONDS(APP_CMD_CTRL_WAIT_BETWEEN_SUBSCRIPTION_REQUESTS_IN_MS));

		// configuration subscribe
		if(RETCODE_OK == retcode) retcode = AppMqtt_Subscribe(&(appCmdCtrl_SubscribeInfosConfigurationsArray[i]));

		if(RETCODE_OK == retcode) vTaskDelay(MILLISECONDS(APP_CMD_CTRL_WAIT_BETWEEN_SUBSCRIPTION_REQUESTS_IN_MS));

		if(RETCODE_OK != retcode) break;

	} // for

    return retcode;
}
/**
 * @brief Deletes subscriptions with one call.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: return from @ref AppMqtt_Unsubscribe()
 */
Retcode_T appCmdCtrl_DeleteSubscriptions(void) {

	Retcode_T retcode = RETCODE_OK;

	const char * topicsArray[AppCmdCtrl_TopicType_Max * 2];
	uint8_t k = 0;

	for(int i=0; i < AppCmdCtrl_TopicType_Max; i++) {
		if(appCmdCtrl_SubscribeInfosCommandsArray[i].topic) {
			topicsArray[k++] = appCmdCtrl_SubscribeInfosCommandsArray[i].topic;
		}
		if(appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic) {
			topicsArray[k++] = appCmdCtrl_SubscribeInfosConfigurationsArray[i].topic;
		}
	}

	if(k>0) retcode = AppMqtt_Unsubscribe(k, topicsArray);

	return retcode;
}
/**
 * @brief Sends the response to a command or configuration instruction using @ref AppStatus_SendStatusMessage().
 * Finishes instruction processing.
 * @param[in] responseMsgPtr: the response message to send.
 */
static void appCmdCtrl_SendResponse(AppStatusMessage_T * responseMsgPtr) {

	AppStatus_SendStatusMessage(responseMsgPtr);

	appCmdCtrl_NotifyInstructionProcessingFinished();
}
/**
 * @brief Process new broker configuration instruction.
 * @details Validates new broker configuration by calling @ref AppRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON() and applies the new
 * configuration by calling @ref applyNewRuntimeConfiguration().
 * Sends response message (failed or success) to calling application.
 * @param[in] inMessage: the received configuration message
 * @param[in] responseMsgPtr: the partially filled response message
 * @param[in] delay2ApplyInstructionTicks: the ticks to delay the processing of the instruction
 * @exception Retcode_RaiseError: result of @ref applyNewRuntimeConfiguration() if not RETCODE_OK
 */
static void appCmdCtrl_ProcessMqttBrokerConnectionConfig(cJSON * inMessage, AppStatusMessage_T * responseMsgPtr, TickType_t delay2ApplyInstructionTicks) {

	AppRuntimeConfig_MqttBrokerConnectionConfig_T * newConfigPtr = AppRuntimeConfig_CreateMqttBrokerConnectionConfig();

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(inMessage, newConfigPtr);

	if(!statusPtr->success) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, statusPtr->descrCode);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, statusPtr->details);
		appCmdCtrl_SendResponse(responseMsgPtr);

		AppRuntimeConfig_DeleteStatus(statusPtr);
		cJSON_Delete(inMessage);
		return;
	}
	AppRuntimeConfig_DeleteStatus(statusPtr);
	cJSON_Delete(inMessage);

	// apply the new runtime configuration
	Retcode_T retcode = applyNewRuntimeConfiguration(AppRuntimeConfig_Element_mqttBrokerConnectionConfig, newConfigPtr, delay2ApplyInstructionTicks);
	if(RETCODE_OK == retcode) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
		appCmdCtrl_SendResponse(responseMsgPtr);
	}
	else {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddRetcode(responseMsgPtr, retcode);
		appCmdCtrl_SendResponse(responseMsgPtr);

		Retcode_RaiseError(retcode);
	}
}
/**
 * @brief Process new topic configuration.
 * @details Validates the config by calling @ref AppRuntimeConfig_PopulateAndValidateTopicConfigFromJSON() and applies the new configuration by calling
 * @ref applyNewRuntimeConfiguration().
 * Sends response message (success or failed) to calling application.
 * @param[in] inMessage: the received configuration message
 * @param[in] responseMsgPtr: the partially filled response message
 * @param[in] delay2ApplyInstructionTicks: the ticks to delay the processing of the instruction
 * @exception Retcode_RaiseError: result of @ref applyNewRuntimeConfiguration() if not RETCODE_OK
 */
static void appCmdCtrl_ProcessTopicConfig(cJSON * inMessage, AppStatusMessage_T * responseMsgPtr, TickType_t delay2ApplyInstructionTicks) {

	AppRuntimeConfig_TopicConfig_T * newConfigPtr = AppRuntimeConfig_CreateTopicConfig();

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(inMessage, newConfigPtr);

	if(!statusPtr->success) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, statusPtr->descrCode);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, statusPtr->details);
		appCmdCtrl_SendResponse(responseMsgPtr);

		AppRuntimeConfig_DeleteStatus(statusPtr);
		cJSON_Delete(inMessage);
		return;
	}
	AppRuntimeConfig_DeleteStatus(statusPtr);
	cJSON_Delete(inMessage);

	// apply the new runtime configuration
	Retcode_T retcode = applyNewRuntimeConfiguration(AppRuntimeConfig_Element_topicConfig, newConfigPtr, delay2ApplyInstructionTicks);
	if(RETCODE_OK == retcode) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
		appCmdCtrl_SendResponse(responseMsgPtr);
	}
	else {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddRetcode(responseMsgPtr, retcode);
		appCmdCtrl_SendResponse(responseMsgPtr);

		Retcode_RaiseError(retcode);
	}
}
/**
 * @brief Process new status configuration.
 * @details Validates the config by calling @ref AppRuntimeConfig_PopulateAndValidateStatusConfigFromJSON() and applies the new configuration by calling
 * @ref applyNewRuntimeConfiguration().
 * Sends response message (success or failed) to calling application.
 * @param[in] inMessage: the received configuration message
 * @param[in] responseMsgPtr: the partially filled response message
 * @param[in] delay2ApplyInstructionTicks: the ticks to delay the processing of the instruction
 * @exception Retcode_RaiseError: result of @ref applyNewRuntimeConfiguration() if not RETCODE_OK
 */
static void appCmdCtrl_ProcessStatusConfig(cJSON * inMessage, AppStatusMessage_T * responseMsgPtr, TickType_t delay2ApplyInstructionTicks) {

	AppRuntimeConfig_StatusConfig_T * newConfigPtr = AppRuntimeConfig_CreateStatusConfig();

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(inMessage, newConfigPtr);

	if(!statusPtr->success) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, statusPtr->descrCode);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, statusPtr->details);
		appCmdCtrl_SendResponse(responseMsgPtr);

		AppRuntimeConfig_DeleteStatus(statusPtr);
		cJSON_Delete(inMessage);
		return;
	}
	AppRuntimeConfig_DeleteStatus(statusPtr);
	cJSON_Delete(inMessage);

	// apply the new runtime configuration
	Retcode_T retcode = applyNewRuntimeConfiguration(AppRuntimeConfig_Element_statusConfig, newConfigPtr, delay2ApplyInstructionTicks);
	if(RETCODE_OK == retcode) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
		appCmdCtrl_SendResponse(responseMsgPtr);
	}
	else {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddRetcode(responseMsgPtr, retcode);
		appCmdCtrl_SendResponse(responseMsgPtr);

		Retcode_RaiseError(retcode);
	}
}
/**
 * @brief Process new telemetry configuration.
 * @details Validates the config by calling @ref AppRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON() and applies the new configuration by calling
 * @ref applyNewRuntimeConfiguration().
 * Sends response message (success or failed) to calling application.
 * @param[in] inMessage: the received configuration message
 * @param[in] responseMsgPtr: the partially filled response message
 * @param[in] delay2ApplyInstructionTicks: the ticks to delay the processing of the instruction
 * @exception Retcode_RaiseError: result of @ref applyNewRuntimeConfiguration() if not RETCODE_OK
 */
static void appCmdCtrl_ProcessTelemetryConfig(cJSON * inMessage, AppStatusMessage_T * responseMsgPtr, TickType_t delay2ApplyInstructionTicks) {

	AppRuntimeConfig_TelemetryConfig_T * newConfigPtr = AppRuntimeConfig_CreateTelemetryConfig();
	AppRuntimeConfig_TelemetryRTParams_T * newTelemetryRtParamsPtr = AppRuntimeConfig_CreateTelemetryRTParams();

	AppRuntimeConfigStatus_T * statusPtr
			= AppRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(inMessage, newConfigPtr, newTelemetryRtParamsPtr);

	if(!statusPtr->success) {

		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, statusPtr->descrCode);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, statusPtr->details);
		appCmdCtrl_SendResponse(responseMsgPtr);

		AppRuntimeConfig_DeleteStatus(statusPtr);
		cJSON_Delete(inMessage);
		return;
	}
	AppRuntimeConfig_DeleteStatus(statusPtr);
	cJSON_Delete(inMessage);

	// apply the new runtime configuration
	Retcode_T retcode = applyNewRuntimeConfiguration(AppRuntimeConfig_Element_targetTelemetryConfig, newConfigPtr, delay2ApplyInstructionTicks);
	if(RETCODE_OK == retcode) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
		appCmdCtrl_SendResponse(responseMsgPtr);
	}
	else {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddRetcode(responseMsgPtr, retcode);
		appCmdCtrl_SendResponse(responseMsgPtr);

		Retcode_RaiseError(retcode);
	}

}
/**
 * @brief Processes a new instruction, either command or configuration.
 * @param[in] requestType: command or configuration
 * @param[in] inMessage: the instruction message
 * @param[in] responseMsgPtr: the partially filled response message
 * @param[in] delay2ApplyInstructionTicks: the ticks to delay the processing of the instruction
 * @exception Retcode_RaiseError: result of @ref appCmdCtrl_executeCommand() if not RETCODE_OK
 */
static void appCmdCtrl_ProcessInstruction(AppCmdCtrlRequestType_T requestType, cJSON * inMessage, AppStatusMessage_T * responseMsgPtr, TickType_t delay2ApplyInstructionTicks) {

	switch (requestType) {
		case AppCmdCtrl_RequestType_Configuration: {

			// extract the config element to be set - mandatory for all configuration messages
			// however, for backwards compatibility, set to target telemetry by default
			AppRuntimeConfig_ConfigElement_T configElement = AppRuntimeConfig_Element_targetTelemetryConfig;
			cJSON * elementJsonHandle = cJSON_GetObjectItem(inMessage, "type");
			if(elementJsonHandle != NULL) {
				const char * elementStr = elementJsonHandle->valuestring;
				if (strstr(elementStr, "topic") != NULL) configElement = AppRuntimeConfig_Element_topicConfig;
				else if (strstr(elementStr, "mqttBrokerConnection") != NULL) configElement = AppRuntimeConfig_Element_mqttBrokerConnectionConfig;
				else if (strstr(elementStr, "status") != NULL) configElement = AppRuntimeConfig_Element_statusConfig;
				else if (strstr(elementStr, "telemetry") != NULL) configElement = AppRuntimeConfig_Element_targetTelemetryConfig;
				else {
					AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
					AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_UnknownConfigType);
					AppStatus_CmdCtrl_AddDetails(responseMsgPtr, elementStr);
					AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
					appCmdCtrl_SendResponse(responseMsgPtr);

					return;
				}
			}

			switch(configElement) {
			case AppRuntimeConfig_Element_topicConfig:
				appCmdCtrl_ProcessTopicConfig(inMessage, responseMsgPtr, delay2ApplyInstructionTicks);
				break;
			case AppRuntimeConfig_Element_mqttBrokerConnectionConfig:
				appCmdCtrl_ProcessMqttBrokerConnectionConfig(inMessage, responseMsgPtr, delay2ApplyInstructionTicks);
				break;
			case AppRuntimeConfig_Element_statusConfig:
				appCmdCtrl_ProcessStatusConfig(inMessage, responseMsgPtr, delay2ApplyInstructionTicks);
				break;
			case AppRuntimeConfig_Element_targetTelemetryConfig:
				appCmdCtrl_ProcessTelemetryConfig(inMessage, responseMsgPtr, delay2ApplyInstructionTicks);
				break;
			default: assert(0);
			}
		}
		break;
		case AppCmdCtrl_RequestType_Command: {
			cJSON * commandJsonHandle = cJSON_GetObjectItem(inMessage, "command");
			if(commandJsonHandle == NULL) {

				AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
				AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_MissingMandatoryElementCommand);
				AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
				appCmdCtrl_SendResponse(responseMsgPtr);

				return;
			}

			// select the command type
			AppCmdCtrl_CommandType_T commandType;

			if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SUSPEND_TELEMETRY) ) commandType = AppCmdCtrl_CommandType_SuspendTelemetry;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_RESUME_TELEMETRY) ) commandType = AppCmdCtrl_CommandType_ResumeTelemetry;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_FULL_STATUS) ) commandType = AppCmdCtrl_CommandType_SendFullStatus;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_SHORT_STATUS) ) commandType = AppCmdCtrl_CommandType_SendShortStatus;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_VERSION_INFO) ) commandType = AppCmdCtrl_CommandType_SendVersionInfo;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_ACTIVE_TELEMETRY_PARAMS) ) commandType = AppCmdCtrl_CommandType_SendActiveTelemetryParams;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_ACTIVE_RUNTIME_CONFIG) ) commandType = AppCmdCtrl_CommandType_SendActiveRuntimeConfig;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_SEND_RUNTIME_CONFIG_FILE) ) commandType = AppCmdCtrl_CommandType_SendRuntimeConfigFile;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_DELETE_RUNTIME_CONFIG_FILE) ) commandType = AppCmdCtrl_CommandType_DeleteRuntimeConfigFile;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_PERSIST_ACTIVE_CONFIG) ) commandType = AppCmdCtrl_CommandType_PersistActiveConfig;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_TRIGGER_SAMPLE_ERROR) ) commandType = AppCmdCtrl_CommandType_TriggerSampleError;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_TRIGGER_SAMPLE_FATAL_ERROR) ) commandType = AppCmdCtrl_CommandType_TriggerSampleFatalError;
			else if(NULL != strstr(commandJsonHandle->valuestring, COMMAND_REBOOT) ) commandType = AppCmdCtrl_CommandType_Reboot;
			else {
				// unknown command
				AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
				AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_UnknownCommand);
				AppStatus_CmdCtrl_AddDetails(responseMsgPtr, commandJsonHandle->valuestring);
				AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
				appCmdCtrl_SendResponse(responseMsgPtr);

				return;
			}

			// special treatment for reboot command

			if(commandType == AppCmdCtrl_CommandType_Reboot) {
				// for REBOOT command, send the status response first
				AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
				appCmdCtrl_SendResponse(responseMsgPtr);

				vTaskDelay(delay2ApplyInstructionTicks);
				BSP_Board_SoftReset();
			}

			Retcode_T retcode = appCmdCtrl_executeCommand(commandType, delay2ApplyInstructionTicks, responseMsgPtr->exchangeId);
			if(RETCODE_OK == retcode) {
				AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Success);
				appCmdCtrl_SendResponse(responseMsgPtr);
			}
			else {
				AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
				AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_ExecutingCommand);
				AppStatus_CmdCtrl_AddDetails(responseMsgPtr, commandJsonHandle->valuestring);
				AppStatus_CmdCtrl_AddRetcode(responseMsgPtr, retcode);
				appCmdCtrl_SendResponse(responseMsgPtr);

				Retcode_RaiseError(retcode);
			}

			cJSON_Delete(inMessage);
		}
		break;
		default: assert(0); //cannot happen
	}
}
/**
 * @brief Preprocess the subscription callback. Enqueued from @ref appCmdCtrl_SubscriptionCallBack().
 * @details Validates the topic contains either 'configuration' or 'command' to determine the instruction type.
 * Parses the incoming message as JSON and sends a failed response if not successful.
 * Extracts the exchangeId from the incoming message (sends failed response if not successful).
 * Extracts optional 'tags' element from the incoming message.
 * Creates the response message with exchangeId, tags.
 * Finally, checks if module is still busy processing a previous instruction and sends a failed reponse.
 * Calls @ref appCmdCtrl_ProcessInstruction() to process the instruction.
 *
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_CMD_CTRL_LISTENING_TO_WRONG_TOPICS)
 *
 */
static void appCmdCtrl_PreprocessSubscriptionCallback(void * paramsPtr, uint32_t processTypeParam) {

	AppCmdCtrl_ProcessType_T processType = (AppCmdCtrl_ProcessType_T) processTypeParam;

	AppXDK_MQTT_IncomingDataCallbackParam_T * subscribeParamsPtr = (AppXDK_MQTT_IncomingDataCallbackParam_T *) paramsPtr;

	AppCmdCtrlRequestType_T requestType = AppCmdCtrl_RequestType_NULL;

	if (strstr(subscribeParamsPtr->topic, "configuration") != NULL) {
		requestType = AppCmdCtrl_RequestType_Configuration;
	} else if (strstr(subscribeParamsPtr->topic, "command") != NULL) {
		requestType = AppCmdCtrl_RequestType_Command;
	} else {
		// should never occur, otherwise we are listening to the wrong topics
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_CMD_CTRL_LISTENING_TO_WRONG_TOPICS));
	}

	AppStatusMessage_T * responseMsgPtr = AppStatus_CmdCtrl_CreateMessage(requestType);

	// create a null terminated payload for JSON parser
	// required with the errorPtr and printing the string
	char * payloadStr = malloc(subscribeParamsPtr->payloadLength + 1);
	strncpy(payloadStr, subscribeParamsPtr->payload, subscribeParamsPtr->payloadLength);
	payloadStr[subscribeParamsPtr->payloadLength] = '\0';
	cJSON *inMessage = cJSON_Parse(payloadStr);
	if (!inMessage) {

		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_ErrorParsingJsonBefore);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, cJSON_GetErrorPtr());
		AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", cJSON_CreateString(payloadStr));
		appCmdCtrl_SendResponse(responseMsgPtr);

		free(payloadStr);
		AppMqtt_FreeSubscribeCallbackParams(subscribeParamsPtr);
		return;
	}
	//free the parameters
	AppMqtt_FreeSubscribeCallbackParams(subscribeParamsPtr);
	free(payloadStr);

	//extract the exchangeId - mandatory for all commands & configuration messages
	cJSON * exchangeIdJsonHandle = cJSON_GetObjectItem(inMessage, "exchangeId");
	if(exchangeIdJsonHandle == NULL) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_MissingMandatoryElement);
		AppStatus_CmdCtrl_AddDetails(responseMsgPtr, "exchangeId");
		AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
		appCmdCtrl_SendResponse(responseMsgPtr);

		return;
	}
	AppStatus_CmdCtrl_AddExchangeId(responseMsgPtr, exchangeIdJsonHandle->valuestring);

	//optional tags
	cJSON * tagsJsonHandle = cJSON_GetObjectItem(inMessage, "tags");
	if(tagsJsonHandle != NULL) {
		AppStatus_CmdCtrl_AddTags(responseMsgPtr, tagsJsonHandle);
	}

	// extract the delay
	int delaySeconds = 1;
	cJSON * delayJsonHandle = cJSON_GetObjectItem(inMessage, "delay");
	if(delayJsonHandle != NULL) {
		delaySeconds = delayJsonHandle->valueint;
		if(delaySeconds < 0) {

			AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
			AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_DelayIsNegative);
			AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
			appCmdCtrl_SendResponse(responseMsgPtr);

			return;
		} else if(delaySeconds > 10 ) {
			AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
			AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_DelayGt10);
			AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
			appCmdCtrl_SendResponse(responseMsgPtr);

			return;
		}
	}
	uint32_t delay2ApplyInstructionTicks = SECONDS(delaySeconds);

	// send status failed if still busy processing previous instruction
	if(AppCmdCtrl_ProcessType_RejectBusyWithReply == processType ) {
		AppStatus_CmdCtrl_AddStatusCode(responseMsgPtr, AppStatusMessage_Status_Failed);
		AppStatus_CmdCtrl_AddDescrCode(responseMsgPtr, AppStatusMessage_Descr_Discarding_StillProcessingPreviousInstruction);
		AppStatus_AddStatusItem(responseMsgPtr, "originalPayload", inMessage);
		appCmdCtrl_SendResponse(responseMsgPtr);

		return;
	}

	appCmdCtrl_ProcessInstruction(requestType, inMessage, responseMsgPtr, delay2ApplyInstructionTicks);

}

static uint32_t subscriptionCallBackBusyCounter = 0; /**< counting active instructions being processed */
/**
 * @brief The global subscription callback.
 * @details Blocks instruction processing and enqueues @ref appCmdCtrl_PreprocessSubscriptionCallback().
 * Allows only 1 instruction to be processed at a time, enqueues the 2. instruction.
 * Any more instructions are discarded, providing (some) protection against too many instructions sent in close sequence.
 * @details type: @ref AppXDK_MQTT_IncomingDataCallback_Func_T()
 * @param[in] params: the incoming subscription parameters.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_CMD_CTRL_FAILED_TO_ENQUEUE_INSTRUCTION_PROCESSING)
 */
static void appCmdCtrl_SubscriptionCallBack(AppXDK_MQTT_IncomingDataCallbackParam_T * params) {

	AppCmdCtrl_ProcessType_T processType = AppCmdCtrl_ProcessType_Process;

	if(pdTRUE != xSemaphoreTake(appCmdCtrl_InstructionProcesssingInProgressSemaphoreHandle, MILLISECONDS(APP_CMD_CTRL_INSTRUCTION_PROCESSING_IN_PROGRESS_SEMAPHORE_WAIT_IN_MS)) ) {
		subscriptionCallBackBusyCounter++;
		processType = AppCmdCtrl_ProcessType_RejectBusyWithReply;
	} else {
		subscriptionCallBackBusyCounter = 0;
		processType = AppCmdCtrl_ProcessType_Process;
	}

	if(subscriptionCallBackBusyCounter > 2) {
		AppMqtt_FreeSubscribeCallbackParams(params);
		return;
	}
	// enqueue processing
	Retcode_T retcode = CmdProcessor_Enqueue(appCmdCtrl_ProcessorHandle, appCmdCtrl_PreprocessSubscriptionCallback, params, processType);
	if(RETCODE_OK != retcode) Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_CMD_CTRL_FAILED_TO_ENQUEUE_INSTRUCTION_PROCESSING));
}

/**@} */
/** ************************************************************************* */

