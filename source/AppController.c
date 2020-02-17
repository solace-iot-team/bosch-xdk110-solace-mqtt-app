/*
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for application development by Licensee. 
* Fitness and suitability of the example code for any use within application developed by Licensee need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
* Licensee shall be responsible for conducting the development of its applications as well as integration of parts of the example code into such applications, taking into account the state of the art of technology and any statutory regulations and provisions applicable for such applications. Compliance with the functional system requirements and testing there of (including validation of information/data security aspects and functional safety) and release shall be solely incumbent upon Licensee. 
* For the avoidance of doubt, Licensee shall be responsible and fully liable for the applications and any distribution of such applications into the market.
* 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are 
* met:
* 
*     (1) Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer. 
* 
*     (2) Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.  
*     
*     (3)The name of the author may not be used to
*     endorse or promote products derived from this software without
*     specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
*  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
*  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
*  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*/
/*----------------------------------------------------------------------------*/
/**
 * @defgroup AppController AppController
 * @{
 *
 * @brief Solace XDK110 application controller.
 *
 * @details Application controller for the Solace App.
 * @details Controls the initialization, setup and enable functions of all modules.
 * @details Controls the overall flow of the app: excuting commands and configuration requests, telemetry sampling and publishing, re-connects to the broker, etc.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_CONTROLLER

#include "AppController.h"

#include "XDK_LED.h"
#include "BCDS_CmdProcessor.h"
#include "XDK_ServalPAL.h"
#include "BCDS_WlanNetworkConnect.h"

#include "AppTimestamp.h"
#include "AppTelemetryQueue.h"
#include "AppMisc.h"
#include "AppCmdCtrl.h"
#include "AppRuntimeConfig.h"
#include "AppTelemetrySampling.h"
#include "AppTelemetryPublish.h"
#include "AppMqtt.h"
#include "AppButtons.h"
#include "AppStatus.h"

/* constants */
#define APP_CONTROLLER_WLAN_RECONNECT_MAX_TRIES			(UINT8_C(50)) /**< number of reconnect tries, WLAN */
#define APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS			(UINT8_C(5000)) /**< wait between reconnect tries, WLAN */

#define APP_CONTROLLER_MQTT_RECONNECT_MAX_TRIES			(UINT8_C(10)) /**< number of reconnect tries, MQTT broker */
#define APP_CONTROLLER_MQTT_RECONNECT_WAIT_MS			(UINT8_C(5000)) /**< wait between reconnect tries, MQTT broker */

/* variables */
static AppTimestamp_T appController_BootTimestamp; /**< save the boot timestamp for status reporting */

/* telemetry tasks management protection */
static SemaphoreHandle_t appController_TelemetryTasksSemaphoreHandle = NULL; /**< semaphore handle to protect access to telemetry tasks (sampling & publishing) */
#define APP_CONTROLLER_TAKE_TELEMETRY_TASKS_SEMAPHORE_WAIT_IN_MS		UINT32_C(10) /**< wait time to take semaphore for telemetry tasks handling */

/* internal state management */
static SemaphoreHandle_t appController_InstructionsSemaphoreHandle = NULL; /**< semaphore for internal state management */
#define APP_CONTROLLER_BLOCK_INSTRUCTIONS_SEMAPHORE_WAIT_IN_MS		UINT32_C(10000) /**< wait time to take the instructions semaphore */
/**
 * @brief internal states
 */
enum AppController_State_E {
	AppController_State_Not_Ready = -1,
	AppController_State_Ready,
	AppController_State_Reconnecting,
	AppController_State_ApplyingNewRuntimeConfiguration,
	AppController_State_ExecutingCommand,
};
typedef enum AppController_State_E AppController_State_T; /**< typedef for #AppController_State_E */

static AppController_State_T appController_InternalState = AppController_State_Not_Ready; /**< variable for the internal state.*/

/**
 * @brief Sets the new state and blocks instructions by taking the semaphore.
 * @param[in] newState: the new state
 * @return bool: true if new state could be set, false otherwise
 */
static bool appController_BlockInstructions(AppController_State_T newState) {
	if(pdTRUE == xSemaphoreTake(appController_InstructionsSemaphoreHandle, MILLISECONDS(APP_CONTROLLER_BLOCK_INSTRUCTIONS_SEMAPHORE_WAIT_IN_MS)) ) {
		appController_InternalState = newState;
		return true;
	} else return false;
}
/**
 * @brief Sets the new state only if it no instructions in progress.
 * @param[in] newState: the new state
 * @return bool: true if successful, false otherwise
 */
static bool appController_BlockInstructionsIfNotInProgress(AppController_State_T newState) {
	if(pdTRUE == xSemaphoreTake(appController_InstructionsSemaphoreHandle, MILLISECONDS(0)) ) {
		appController_InternalState = newState;
		return true;
	} else return false;
}
/**
 * @brief Sets the state to ready and releases the semaphore.
 */
static void appController_AllowInstructions(void) {
	appController_InternalState = AppController_State_Ready;
	xSemaphoreGive(appController_InstructionsSemaphoreHandle);
}

/* command processors */
static CmdProcessor_T * AppControllerProcessorHandle = NULL; /**< controller processor handle, passed from Main */
static CmdProcessor_T AppCmdCtrlProcessor; 	/**< AppCmdCtrl processor handle, created here */
static CmdProcessor_T ServalCmdProcessor;	/**< Serval processor handle, created here */
static CmdProcessor_T SensorCmdProcessor;	/**< Sensor processor handle, created here */
static CmdProcessor_T ButtonsCmdProcessor;	/**< Buttons processor handle, created here */
static CmdProcessor_T StatusCmdProcessor;	/**< Status processor handle, created here */

static bool appController_targetTelemetryState_isRunning = false; /**< flag to keep track if telemetry tasks are running or not across processing instructions*/

/**
 * @brief Returns if telemetry tasks are running. Either both are running or none is running, otherwise raise a fatal error.
 * @return bool: true if they are running, false it not.
 * @exception Retcode_RaiseError (RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_INCONSISTENT_TELEMETRY_TASKS)
 */
static bool areTelemetryTasksRunning(void) {

	bool isPub = false;
	bool isSampling = false;

	if(pdTRUE == xSemaphoreTake(appController_TelemetryTasksSemaphoreHandle, MILLISECONDS(APP_CONTROLLER_TAKE_TELEMETRY_TASKS_SEMAPHORE_WAIT_IN_MS))) {

		isPub  = AppTelemetryPublish_isTaskRunning();
		isSampling = AppTelemetrySampling_isTaskRunning();

		xSemaphoreGive(appController_TelemetryTasksSemaphoreHandle);
	} else assert(0);

	if(isPub && isSampling) return true;
	if(!isPub && !isSampling) return false;

	Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_INCONSISTENT_TELEMETRY_TASKS));
	return false;
}
/**
 * @brief Creates the telemetry tasks and the internal queue.
 * @return Retcode_T: RETCODE_OK if created/already running, otherwise returns retcode from called functions.
 */
static Retcode_T appController_CreateTelemetryTasks(void) {

	Retcode_T retcode = RETCODE_OK;

	if(areTelemetryTasksRunning()) return RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appController_TelemetryTasksSemaphoreHandle, MILLISECONDS(APP_CONTROLLER_TAKE_TELEMETRY_TASKS_SEMAPHORE_WAIT_IN_MS))) {

		if (RETCODE_OK == retcode) retcode = AppTelemetryQueue_Prepare();

		if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_CreateSamplingTask();

		if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_CreatePublishingTask();

		xSemaphoreGive(appController_TelemetryTasksSemaphoreHandle);

	}

	return retcode;
}
/**
 * @brief Suspends (deletes) the telemetry tasks if they are running.
 * @return Retcode_T: RETCODE_OK or retcodes from called functions.
 */
static Retcode_T appController_SuspendTelemetryTasks(void) {

	Retcode_T retcode = RETCODE_OK;

	if(!areTelemetryTasksRunning()) return RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appController_TelemetryTasksSemaphoreHandle, MILLISECONDS(APP_CONTROLLER_TAKE_TELEMETRY_TASKS_SEMAPHORE_WAIT_IN_MS))) {

		if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_DeletePublishingTask();

		if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_DeleteSamplingTask();

		xSemaphoreGive(appController_TelemetryTasksSemaphoreHandle);

	}

	return retcode;
}
/**
 * @brief Applies a new configuration for the MQTT broker.
 * @warning Currently not supported. Sends a warning status message and returns a warning only.
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_UNSUPPORTED_FEATURE);
 */
static Retcode_T applyNewRuntime_MqttBrokerConnectionConfig(AppRuntimeConfig_MqttBrokerConnectionConfig_T * newConfigPtr) {

	BCDS_UNUSED(newConfigPtr);

	#ifdef DEBUG_APP_CONTROLLER
	printf("[INFO] - AppController.applyNewRuntime_MqttBrokerConnectionConfig: applying new mqtt broker connection configuration...\r\n");
	printf("old configuration\r\n");
	AppRuntimeConfig_Print(AppRuntimeConfig_Element_mqttBrokerConnectionConfig, getAppRuntimeConfigPtr()->mqttBrokerConnectionConfigPtr);
	printf("new configuration\r\n");
	AppRuntimeConfig_Print(AppRuntimeConfig_Element_mqttBrokerConnectionConfig, newConfigPtr);
	#endif

	// send status that this is not supported currently
	AppStatus_SendStatusMessage(AppStatus_CreateMessage(AppStatusMessage_Status_Warning, AppStatusMessage_Descr_ApplyNewMqttBrokerConnectionConfig_NotSupported, NULL));

	return RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_UNSUPPORTED_FEATURE);
}
/**
 * @brief Applies a new topic configuration. Suspends telemetry & status modules and applies the new configuration to the various modules.
 * Reverts back to old configuration in case of an error and raises a fatal error.
 * @see AppCmdCtrl_ApplyNewRuntimeConfig() for MQTT session subscription handling.
 * @param[in] newConfigPtr: the new topic configuration
 * @return Retcode_T: RETCODE_OK
 * @exception Retcode_RaiseError the non-RETCODE_OK error from the called modules
 * @exception Retcode_RaiseError (RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APPLY_NEW_RUNTIME_CONFIG_TOPIC)
 */
static Retcode_T applyNewRuntime_TopicConfig(AppRuntimeConfig_TopicConfig_T * newConfigPtr) {

	Retcode_T retcode = RETCODE_OK;

	AppRuntimeConfig_TopicConfig_T * oldConfigPtr = AppRuntimeConfig_DuplicateTopicConfig(getAppRuntimeConfigPtr()->topicConfigPtr);

	#ifdef DEBUG_APP_CONTROLLER
	{
		printf("[INFO] - AppController.applyNewRuntime_TopicConfig: applying new topic runtime configuration...\r\n");
		printf("old topic configuration\r\n");
		AppRuntimeConfig_Print(AppRuntimeConfig_Element_topicConfig, oldConfigPtr);
		printf("new topic configuration\r\n");
		AppRuntimeConfig_Print(AppRuntimeConfig_Element_topicConfig, newConfigPtr);
	}
	#endif

	if (RETCODE_OK == retcode) retcode = appController_SuspendTelemetryTasks();

	if (RETCODE_OK == retcode) retcode = AppStatus_SuspendRecurringTask();

	if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, newConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, getAppRuntimeConfigPtr()->topicConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, getAppRuntimeConfigPtr()->topicConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppButtons_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, getAppRuntimeConfigPtr()->topicConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, getAppRuntimeConfigPtr()->topicConfigPtr);

	if (RETCODE_OK != retcode) Retcode_RaiseError(retcode);

	// if the error is because of a disconnect, the disconnect handler will recover
	// otherwise, we probably can't recover, hence: reboot

	if (RETCODE_OK != retcode) {
		if(newConfigPtr->received.applyFlag == AppRuntimeConfig_Apply_Persistent) AppRuntimeConfig_DeleteFile();
		oldConfigPtr->received.applyFlag = AppRuntimeConfig_Apply_Transient;
		AppRuntimeConfig_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, oldConfigPtr);
		AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, oldConfigPtr);

		// the original error
		Retcode_RaiseError(retcode);
		// make sure we send fatal error if not already
		if(RETCODE_SEVERITY_FATAL != Retcode_GetSeverity(retcode)) Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APPLY_NEW_RUNTIME_CONFIG_TOPIC));

	} else {
		AppRuntimeConfig_DeleteTopicConfig(oldConfigPtr);
	}

	return retcode;
}
/**
 * @brief Applies a new status configuration after suspending the recurring status task.
 * @param[in] newConfigPtr: the new status configuration
 * @return Retcode_T: RETCODE_OK, or retcode from called functions.
 */
static Retcode_T applyNewRuntime_StatusConfig(AppRuntimeConfig_StatusConfig_T * newConfigPtr) {

	Retcode_T retcode = RETCODE_OK;

	#ifdef DEBUG_APP_CONTROLLER
	printf("[INFO] - AppController.applyNewRuntime_StatusConfig: applying new status runtime configuration...\r\n");
	printf("old status configuration\r\n");
	AppRuntimeConfig_Print(AppRuntimeConfig_Element_statusConfig, getAppRuntimeConfigPtr()->statusConfigPtr);
	printf("new status configuration\r\n");
	AppRuntimeConfig_Print(AppRuntimeConfig_Element_statusConfig, newConfigPtr);
	#endif

	//ensure the periodic task is not running first
	if (RETCODE_OK == retcode) retcode = AppStatus_SuspendRecurringTask();

	if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_statusConfig, newConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_statusConfig, getAppRuntimeConfigPtr()->statusConfigPtr);

	return retcode;
}

/**
 * @brief Applies a new target telemetry configuration.
 * @note Module @ref AppRuntimeConfig also re-calculates the activeTelemetryRTParams, which are also applied to the various modules.
 * @param[in] newConfigPtr: the new target telemetry configuration
 * @return Retcode_T: RETCODE_OK or the retcode from the called functions.
 */
static Retcode_T applyNewRuntime_TargetTelemetryConfig(AppRuntimeConfig_TelemetryConfig_T * newConfigPtr) {

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = appController_SuspendTelemetryTasks();

    if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_targetTelemetryConfig, newConfigPtr);

    if (RETCODE_OK == retcode) retcode = AppTelemetryQueue_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, getAppRuntimeConfigPtr()->activeTelemetryRTParamsPtr);

    if (RETCODE_OK == retcode) retcode = AppTelemetryPayload_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_targetTelemetryConfig, newConfigPtr);

	if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, getAppRuntimeConfigPtr()->activeTelemetryRTParamsPtr);

	if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_targetTelemetryConfig, newConfigPtr);

    if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, getAppRuntimeConfigPtr()->activeTelemetryRTParamsPtr);

	return retcode;
}

/**
 * @brief Applies a new runtime configuration for the requested element.
 * @details Called by @ref AppCmdCtrl module when a new configuration is sent. The new config is already validated.
 * Blocks all new instructions while running, e.g. commands and configuration messages.
 *
 * @note Call synchronously, don't queue a task in the flow here. Which means it runs in callers command processor.
 *
 * @param[in] configElement: the config element type
 * @param[in] newConfigPtr: the new configuration, already validated
 *
 * @return Retcode_T: RETCODE_OK, or the retcode of the called functions.
 *
 * @note Function is of type: @ref AppControllerApplyRuntimeConfiguration_Func_T
 *
 */
Retcode_T AppController_ApplyNewRuntimeConfiguration(AppRuntimeConfig_ConfigElement_T configElement, void * newConfigPtr) {

	Retcode_T retcode = RETCODE_OK;

	if(!appController_BlockInstructions(AppController_State_ApplyingNewRuntimeConfiguration)) {
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_CONTROLLER_NOT_READY_TO_APPLY_NEW_RUNTIME_CONFIG);
	}

	appController_targetTelemetryState_isRunning = areTelemetryTasksRunning();

	switch(configElement) {
	case AppRuntimeConfig_Element_topicConfig:
		retcode = applyNewRuntime_TopicConfig((AppRuntimeConfig_TopicConfig_T *) newConfigPtr);
		break;
	case AppRuntimeConfig_Element_mqttBrokerConnectionConfig:
		retcode = applyNewRuntime_MqttBrokerConnectionConfig((AppRuntimeConfig_MqttBrokerConnectionConfig_T *) newConfigPtr);
		break;
	case AppRuntimeConfig_Element_statusConfig:
		retcode = applyNewRuntime_StatusConfig((AppRuntimeConfig_StatusConfig_T *) newConfigPtr);
		break;
	case AppRuntimeConfig_Element_activeTelemetryRTParams:
		// these are not set directly but implicitely with targetTelemetryConfig
		assert(0);
		break;
	case AppRuntimeConfig_Element_targetTelemetryConfig:
		retcode = applyNewRuntime_TargetTelemetryConfig((AppRuntimeConfig_TelemetryConfig_T *) newConfigPtr);
		break;
	default: assert(0);
	}

	if (RETCODE_OK == retcode && appController_targetTelemetryState_isRunning) retcode = appController_CreateTelemetryTasks();

	appController_AllowInstructions();

	return retcode;
}
/**
 * @brief Executes a command. Called by @ref AppCmdCtrl when a new command is received. It runs in Callers command processor.
 * The exchangeIdStr is used by the calling application to correlate the command with the responses.
 * Blocks instructions until finished.
 *
 * @note Call synchronously, don't queue a task in the flow here. Which means it runs in callers CmdProcessor.
 *
 * @param[in] commandType: the command
 * @param[in] exchangeIdStr: the exchangeId set by the calling application
 * @return Retcode_T: RETCODE_OK,
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_CONTROLLER_NOT_READY_TO_EXECUTE_COMMAND) if module is busy with other instructions
 * @return Retcode_T: result from called functions.
 *
 * @note Function is of type: @ref AppControllerExecuteCommand_Func_T
 *
 */
Retcode_T AppController_ExecuteCommand(AppCmdCtrl_CommandType_T commandType, const char * exchangeIdStr) {

	assert(exchangeIdStr);

	Retcode_T retcode = RETCODE_OK;

	if(!appController_BlockInstructions(AppController_State_ExecutingCommand)) {
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_CONTROLLER_NOT_READY_TO_EXECUTE_COMMAND);
	}

	switch(commandType) {
	case AppCmdCtrl_CommandType_SuspendTelemetry: {
		retcode = appController_SuspendTelemetryTasks();
	}
	break;
	case AppCmdCtrl_CommandType_ResumeTelemetry: {
		retcode = appController_CreateTelemetryTasks();
	}
	break;
	case AppCmdCtrl_CommandType_SendFullStatus: {
		AppStatus_SendCurrentFullStatus(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_SendShortStatus: {
		AppStatus_SendCurrentShortStatus(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_SendVersionInfo: {
		AppStatus_SendVersionInfo(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_SendActiveTelemetryParams: {
		AppStatus_SendActiveTelemetryParams(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_SendRuntimeConfigFile: {
		AppRuntimeConfig_SendFile(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_SendActiveRuntimeConfig: {
		AppRuntimeConfig_SendActiveConfig(exchangeIdStr);
	}
	break;
	case AppCmdCtrl_CommandType_DeleteRuntimeConfigFile: {
		AppRuntimeConfig_DeleteFile();
	}
	break;
	case AppCmdCtrl_CommandType_PersistActiveConfig: {
		retcode = AppRuntimeConfig_PersistRuntimeConfig2File();
	}
	break;
	case AppCmdCtrl_CommandType_TriggerSampleError: {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_TEST_CODE));
		retcode = RETCODE_OK;
	}
	break;
	case AppCmdCtrl_CommandType_TriggerSampleFatalError: {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_TEST_CODE));
		retcode = RETCODE_OK;
	}
	break;
	default: assert(0);
	}

	if(RETCODE_OK == retcode) appController_targetTelemetryState_isRunning = areTelemetryTasksRunning();

	appController_AllowInstructions();

	return retcode;
}

/**
 * @brief Setup after a disconnect event from the broker.
 * @details Enqueued by @ref appController_MqttBrokerDisconnectCallback(), runs in AppController command processor.
 * Waits until AppController is finished processing any other instructions and then blocks new instruction processing until finished.
 * @details Sequence: <br/>
 * - notifies modules of the disconnect event and suspends telemetry tasks.<br/>
 * - reconnect: <br/>
 *    - checks if WLAN connection still active, waits #APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS milliseconds for #APP_CONTROLLER_WLAN_RECONNECT_MAX_TRIES times until it is<br/>
 *    - connects to the broker #APP_CONTROLLER_MQTT_RECONNECT_MAX_TRIES times with a wait of #APP_CONTROLLER_MQTT_RECONNECT_WAIT_MS before trying again
 *    - if it can't connect, raises a FATAL error <br/>
 * - notifies modules of the reconnect and starts telemetry tasks again.
 *
 * @param[in] param1: unused
 * @param[in] param2: unused
 * @exception Retcode_RaiseError (RETCODE_SEVERITY_FATAL) if it can't get an internet connection / connect to the broker
 * @exception Retcode_RaiseError - with called functions' retcode if not RETCODE_OK
 *
 * @note It can happen that after a reconnect another disconnect event occurs. Hence, all suspend and setup functions must handle being called again.
 */
static void appController_SetupAfterDisconnect(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	AppMisc_UserFeedback_InSetup();

	AppStatus_Stats_IncrementMqttBrokerDisconnectCounter();

	// this message will be sent after re-connection
	AppStatus_SendMqttBrokerDisconnectedMessage();

	if(!appController_BlockInstructionsIfNotInProgress(AppController_State_Reconnecting)) {
		switch(appController_InternalState) {
		case AppController_State_ApplyingNewRuntimeConfiguration:
			printf("[INFO] - appController_SetupAfterDisconnect: currently busy with AppController_State_ApplyingNewRuntimeConfiguration\r\n");
			break;
		case AppController_State_ExecutingCommand:
			printf("[INFO] - appController_SetupAfterDisconnect: currently busy with AppController_State_ExecutingCommand\r\n");
			break;
		case AppController_State_Not_Ready: assert(0); break;
		case AppController_State_Ready: assert(0); break;
		case AppController_State_Reconnecting: assert(0); break;
		default: assert(0); break;
		}
		printf("[INFO] - appController_SetupAfterDisconnect: AppController busy, waiting until it is finished ...\r\n");
		uint8_t counter = 0;
		while(!appController_BlockInstructionsIfNotInProgress(AppController_State_Reconnecting)) {
			vTaskDelay(100);
			if(++counter == 200) Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_CONTROLLER_BUSY_FAILED_TO_SETUP_AFTER_DISCONNECT));
		}
	}

	Retcode_T retcode = RETCODE_OK;

	// suspend modules
	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_NotifyDisconnectedFromBroker();

	if (RETCODE_OK == retcode) retcode = AppStatus_NotifyDisconnectedFromBroker();

	if (RETCODE_OK == retcode) retcode = AppButtons_NotifyDisconnectedFromBroker();

	if (RETCODE_OK == retcode) retcode = appController_SuspendTelemetryTasks();

	// connect
	if (RETCODE_OK == retcode) {

		uint8_t connectTriesCounter = 1;

		WlanNetworkConnect_IpStatus_T nwStatus = WlanNetworkConnect_GetIpStatus();

		if(WLANNWCT_IPSTATUS_CT_AQRD != nwStatus) {
			AppStatus_Stats_IncrementWlanDisconnectCounter();
			// this message will be sent after re-connection
			AppStatus_SendWlanDisconnectedMessage();
		}

	    while (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus && connectTriesCounter < (APP_CONTROLLER_WLAN_RECONNECT_MAX_TRIES+1)) {

	    	printf("[WARNING] - appController_SetupAfterDisconnect : no WLAN connectivity.\r\n");
	    	printf("[INFO] - appController_SetupAfterDisconnect : waiting to reconnect %u millis, attempt %u ...\r\n", APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS, connectTriesCounter);

			// retcode = WLAN_Reconnect();
	    	// this does not work with ServalPal - it gets confused
			// however, WLAN module will reconnect by itself

			nwStatus = WlanNetworkConnect_GetIpStatus();

			vTaskDelay(MILLISECONDS(APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS));

			connectTriesCounter++;

	    }
	    if(WLANNWCT_IPSTATUS_CT_AQRD != nwStatus) retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_WLAN_NOT_CONNECTED);
	    else retcode = RETCODE_OK;
	    if(RETCODE_OK != retcode) {
		    // cannot get internet connection, abort and bootstrap
			retcode = RETCODE(RETCODE_SEVERITY_FATAL, Retcode_GetCode(retcode));
			printf("[FATAL] - appController_SetupAfterDisconnect: connecting to WLAN failed %u times.\r\n", connectTriesCounter);
			Retcode_RaiseError(retcode);
	    }
	    // we have wlan connectivity, now try connect to broker

		connectTriesCounter = 1;
		Retcode_T connectRetcode = AppMqtt_Connect2Broker();

		while(RETCODE_OK != connectRetcode && connectTriesCounter < (APP_CONTROLLER_MQTT_RECONNECT_MAX_TRIES+1)) {

			printf("[WARNING] - appController_SetupAfterDisconnect: AppMqtt_Connect2Broker() failed. \r\n");

			connectTriesCounter++;

			printf("[INFO] - appController_SetupAfterDisconnect: trying to connect again in %u millis, attempt %u ...\r\n", APP_CONTROLLER_MQTT_RECONNECT_WAIT_MS, connectTriesCounter);

			vTaskDelay(MILLISECONDS(APP_CONTROLLER_MQTT_RECONNECT_WAIT_MS));

			connectRetcode = AppMqtt_Connect2Broker();

		}
		if(RETCODE_OK != connectRetcode) {
			// cannot connect to broker, abort and bootstrap
			retcode = RETCODE(RETCODE_SEVERITY_FATAL, Retcode_GetCode(connectRetcode));
			printf("[FATAL] - appController_SetupAfterDisconnect: AppMqtt_Connect2Broker() failed %u times.\r\n", connectTriesCounter);
			Retcode_RaiseError(retcode);
		} else {
			printf("[INFO] - appController_SetupAfterDisconnect: connected successfully, attempt %u.\r\n", connectTriesCounter);
		}
	}

	// setup again

	if (RETCODE_OK == retcode) retcode = AppStatus_NotifyReconnected2Broker();

	if (RETCODE_OK == retcode) retcode = AppButtons_NotifyReconnected2Broker();

	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_NotifyReconnected2Broker();

	if (RETCODE_OK == retcode && appController_targetTelemetryState_isRunning) retcode = appController_CreateTelemetryTasks();

	if (RETCODE_OK == retcode) AppMisc_UserFeedback_Ready();

	if (RETCODE_OK != retcode) Retcode_RaiseError(retcode);

	appController_AllowInstructions();

}
/**
 * @brief Callback from @ref AppMqtt module when connection closed event received.
 * @details Enqueues @ref appController_SetupAfterDisconnect().
 * @exception Retcode_RaiseError RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_CONTROLLER_FAILED_TO_ENQUEUE_DISCONNECT_MANAGER) if failed to enqueue function
 *
 * @note Function is of type: @ref AppMqtt_BrokerDisconnectedControllerCallback_Func_T
 */
static void appController_MqttBrokerDisconnectCallback(void) {
	Retcode_T retcode = RETCODE_OK;

	retcode = CmdProcessor_Enqueue(AppControllerProcessorHandle, appController_SetupAfterDisconnect, NULL, UINT32_C(0));
	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_CONTROLLER_FAILED_TO_ENQUEUE_DISCONNECT_MANAGER));
	}
}
/**
 * @brief Enables the application. Enqueued by #AppController_Setup().
 * Concludes the initialization process.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_CONTROLLER_ENABLE_FAILED)
 */
static void AppController_Enable(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	Retcode_T retcode = RETCODE_OK;

	if(RETCODE_OK == retcode) retcode = WLAN_Enable();

	// tries again and if it fails, module will reboot
	uint8_t connectTriesCounter = 1;
    while (RETCODE_OK != retcode && connectTriesCounter < (APP_CONTROLLER_WLAN_RECONNECT_MAX_TRIES+1)) {
    	printf("[WARNING] - AppController_Enable: no WLAN connectivity.\r\n");
    	printf("[INFO] - AppController_Enable: waiting to reconnect %u millis, attempt %u ...\r\n", APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS, connectTriesCounter);

		retcode = WLAN_Reconnect();

    	vTaskDelay(MILLISECONDS(APP_CONTROLLER_WLAN_RECONNECT_WAIT_MS));
		connectTriesCounter++;
    }

	if (RETCODE_OK == retcode) retcode = ServalPAL_Enable();

	if (RETCODE_OK == retcode) retcode = AppTimestamp_Enable();

	if (RETCODE_OK == retcode) retcode = AppStatus_Enable(appController_BootTimestamp);

	if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_Enable();

	if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_Enable();

	if (RETCODE_OK == retcode) retcode = AppMqtt_Connect2Broker();

	if (RETCODE_OK == retcode) retcode = AppStatus_SendBootStatus();

	if (RETCODE_OK == retcode) {

		AppStatus_SendQueuedMessages();

		AppStatus_CreateRecurringTask();
	}

	if (RETCODE_OK == retcode) retcode = AppButtons_Enable();

	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_Enable(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) {
		if(getAppRuntimeConfigPtr()->targetTelemetryConfigPtr->received.activateAtBootTime) {
			retcode = appController_CreateTelemetryTasks();
			if(RETCODE_OK == retcode) appController_targetTelemetryState_isRunning = true;
		} else appController_targetTelemetryState_isRunning = false;
	}

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_CONTROLLER_ENABLE_FAILED));
	}

	AppMisc_UserFeedback_Ready();

	appController_AllowInstructions();

}
/**
 * @brief Setup of the application. Calls various modules' setup function. Enqueued by #AppController_Init().
 * Enqueues #AppController_Enable().
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_CONTROLLER_SETUP_FAILED)
 */
static void AppController_Setup(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_Setup();

	if (RETCODE_OK == retcode) retcode = AppStatus_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = WLAN_Setup((WLAN_Setup_T *)AppConfig_GetWlanSetupInfoPtr());

	if (RETCODE_OK == retcode) retcode = ServalPAL_Setup(&ServalCmdProcessor);

	if (RETCODE_OK == retcode) retcode = AppTimestamp_Setup(AppConfig_GetSntpSetupInfoPtr());

	if (RETCODE_OK == retcode) retcode = AppMqtt_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_Setup();

	if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = AppTelemetryQueue_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = AppTelemetryPayload_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = AppButtons_Setup(getAppRuntimeConfigPtr());

	if (RETCODE_OK == retcode) retcode = CmdProcessor_Enqueue(AppControllerProcessorHandle, AppController_Enable, NULL, UINT32_C(0));

	if (RETCODE_OK != retcode) {
		// raise the original error and then raise a fatal error
		Retcode_RaiseError(retcode);
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_CONTROLLER_SETUP_FAILED));
	}
}
/**
 * @brief Initialize the application.
 *
 * @details Creates the various processors, reads the configuration file and calls various module setup/init functions.
 * @details Call from Main.c to start the application.
 * @details Enqueues the setup function #AppController_Setup.
 *
 * @param[in]  cmdProcessorHandle : contains the AppController processor handle
 * @param[in]  param2: unused
 *
 * @exception Retcode_RaiseError: (RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_CONTROLLER_INIT_FAILED)
 */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2) {

	BCDS_UNUSED(param2);

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = AppTimestamp_Init();
	// capture the boot timestamp before anything else
	appController_BootTimestamp = AppTimestamp_GetTimestamp(xTaskGetTickCount());

	#ifdef DEBUG_APP_CONTROLLER
	/**
	 * note: is there a way to check if a console is attached?
	 * if console attached, then wait. otherwise we won't see the printfs, it's too fast.
	 * if no console attached, then don't wait.
	 */
	vTaskDelay(2000);
	#endif

	if(cmdProcessorHandle != NULL) AppControllerProcessorHandle = (CmdProcessor_T *) cmdProcessorHandle;
	else retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_CMD_PROCESSOR_IS_NULL);

	// initialize internal state
	if (RETCODE_OK == retcode) {
		appController_InstructionsSemaphoreHandle = xSemaphoreCreateBinary();
		if(appController_InstructionsSemaphoreHandle == NULL) retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appController_InstructionsSemaphoreHandle);
	}
	appController_BlockInstructions(AppController_State_Not_Ready);

	// initialize telemetry tasks handler
	if (RETCODE_OK == retcode) {
		appController_TelemetryTasksSemaphoreHandle = xSemaphoreCreateBinary();
		if(appController_TelemetryTasksSemaphoreHandle == NULL) retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appController_TelemetryTasksSemaphoreHandle);
	}

	// initialize the other processors
	if (RETCODE_OK == retcode) retcode = CmdProcessor_Initialize(&AppCmdCtrlProcessor, (char *) "AppCmdCtrlProcessor", APP_CMD_CTRL_PROCESSOR_PRIORITY, APP_CMD_CTRL_PROCESSOR_STACK_SIZE, APP_CMD_CTRL_PROCESSOR_QUEUE_LEN);
	if (RETCODE_OK == retcode) retcode = CmdProcessor_Initialize(&ServalCmdProcessor, (char *) "ServalProcessor", SERVAL_PROCESSOR_PRIORITY, SERVAL_PROCESSOR_STACK_SIZE, SERVAL_PROCESSOR_QUEUE_LEN);
	if (RETCODE_OK == retcode) retcode = CmdProcessor_Initialize(&SensorCmdProcessor, (char *) "SensorProcessor", SENSOR_PROCESSOR_PRIORITY, SENSOR_PROCESSOR_STACK_SIZE, SENSOR_PROCESSOR_QUEUE_LEN);
	if (RETCODE_OK == retcode) retcode = CmdProcessor_Initialize(&ButtonsCmdProcessor, (char *) "ButtonsProcessor", APP_BUTTONS_PROCESSOR_PRIORITY, APP_BUTTONS_PROCESSOR_STACK_SIZE, APP_BUTTONS_PROCESSOR_QUEUE_LEN);
	if (RETCODE_OK == retcode) retcode = CmdProcessor_Initialize(&StatusCmdProcessor, (char *) "StatusProcessor", APP_STATUS_PROCESSOR_PRIOIRTY, APP_STATUS_PROCESSOR_STACK_SIZE, APP_STATUS_PROCESSOR_QUEUE_LEN);

	// set up and enable LEDs here already for feedback
	if (RETCODE_OK == retcode) retcode = LED_Setup();
	if (RETCODE_OK == retcode) retcode = LED_Enable();
	if (RETCODE_OK == retcode) AppMisc_UserFeedback_InSetup();

	AppMisc_InitDeviceId();

	AppMisc_PrintVersionInfo();

	// now init the AppStatus first since it may be used by all other modules
	if (RETCODE_OK == retcode) retcode = AppStatus_Init(AppMisc_GetDeviceId(), &StatusCmdProcessor);

	if (RETCODE_OK == retcode) retcode = AppConfig_Init(AppMisc_GetDeviceId());

	if (RETCODE_OK == retcode) retcode = AppTelemetryQueue_Init();

	if (RETCODE_OK == retcode) retcode = AppTelemetryPayload_Init(AppMisc_GetDeviceId());

	if (RETCODE_OK == retcode) retcode = AppTelemetryPublish_Init(AppMisc_GetDeviceId(), APP_TELEMETRY_PUBLISHING_TASK_PRIORITY, APP_TELEMETRY_PUBLISHING_TASK_STACK_SIZE);

	if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_Init(AppMisc_GetDeviceId(), APP_TELEMETRY_SAMPLING_TASK_PRIORITY, APP_TELEMETRY_SAMPLING_TASK_STACK_SIZE, &SensorCmdProcessor);

	if (RETCODE_OK == retcode) retcode = AppRuntimeConfig_Init(AppMisc_GetDeviceId());

	if (RETCODE_OK == retcode) retcode = AppCmdCtrl_Init(	AppMisc_GetDeviceId(),
															AppConfig_GetMqttConnectInfoPtr()->isCleanSession,
															&AppCmdCtrlProcessor,
															AppController_ApplyNewRuntimeConfiguration,
															AppController_ExecuteCommand);

	if (RETCODE_OK == retcode) retcode = AppButtons_Init(AppMisc_GetDeviceId(), &ButtonsCmdProcessor);

	if (RETCODE_OK == retcode) retcode = AppMqtt_Init(AppMisc_GetDeviceId(), appController_MqttBrokerDisconnectCallback, AppCmdCtrl_GetGlobalSubscriptionCallback());

	if (RETCODE_OK == retcode) retcode = CmdProcessor_Enqueue(AppControllerProcessorHandle, AppController_Setup, NULL, UINT32_C(0));

	if (RETCODE_OK != retcode) {
		// raise the original error and then raise a fatal error
		Retcode_RaiseError(retcode);
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_CONTROLLER_INIT_FAILED));
	}
}

/**@} */
/** ************************************************************************* */
