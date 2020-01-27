/*
 * AppTelemetryPublish.c
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppTelemetryPublish AppTelemetryPublish
 * @{
 *
 * @brief Module to publish telemetry / sensor samples based on configured intervals.
 * @see AppTelemetrySampling
 * @see AppTelemetryPayload
 * @see AppTelemetryQueue
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_TELEMETRY_PUBLISH

#include "AppTelemetryPublish.h"
#include "AppMqtt.h"
#include "AppRuntimeConfig.h"
#include "AppConfig.h"
#include "AppMisc.h"
#include "AppTelemetryQueue.h"
#include "AppStatus.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static xTaskHandle appTelemetryPublish_TaskHandle = NULL; /**< the publishing task task handle */
static uint32_t appTelemetryPublish_PublishTaskPriority = 1; /**< the task priority with default value */
static uint32_t appTelemetryPublish_PublishTaskStackSize = 1024; /**< the task stack size with default value */

static SemaphoreHandle_t appTelemetryPublish_TaskSemaphoreHandle = NULL; /**< the task semaphore */
#define APP_TELEMETRY_PUBLISHING_TASK_INTERNAL_WAIT_TICKS			UINT32_C(10) /**< wait ticks for semaphore to start task */
#define APP_TELEMETRY_PUBLISHING_TASK_DELETE_INTERNAL_WAIT_TICKS	UINT32_C(5000) /**< wait ticks for semaphore to delete task */

/**
 * @brief Publish information structure.
 */
static AppXDK_MQTT_Publish_T appTelemetryPublish_MqttPublishInfo = {
	.topic = NULL,
	.qos = 0UL,
	.payload = NULL,
	.payloadLength = 0UL,
};

static TickType_t appTelemetryPublish_publishPeriodcityMillis = 1000; /**< internal configuration for publish interval in millis */

static const char * appTelemetryPublish_DeviceId = NULL; /**< internal device id */


/* forward declarations */
static void appTelemetryPublishing_TelemetryPublishTask(void* pvParameters);

/**
 * @brief Initialize the module.
 *
 * @param[in] deviceId : the device id. keeps a local copy.
 * @param[in] publishTaskPriority: the task priority
 * @param[in] publishTaskStackSize: the task stack size
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 */
Retcode_T AppTelemetryPublish_Init(const char * deviceId, uint32_t publishTaskPriority, uint32_t publishTaskStackSize) {

	assert(deviceId);

	Retcode_T retcode = RETCODE_OK;

	appTelemetryPublish_DeviceId = copyString(deviceId);

	appTelemetryPublish_PublishTaskPriority = publishTaskPriority;

	appTelemetryPublish_PublishTaskStackSize = publishTaskStackSize;

	appTelemetryPublish_TaskSemaphoreHandle = xSemaphoreCreateBinary();
	if(appTelemetryPublish_TaskSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appTelemetryPublish_TaskSemaphoreHandle);

	return retcode;

}
/**
 * @brief Setup the module.
 *
 * @param[in] configPtr : the complete runtime config. Module requires targetTelemetryConfigPtr, topicConfigPtr, activeTelemetryRTParamsPtr
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcodes from @ref AppTelemetryPublish_ApplyNewRuntimeConfig()
 */
Retcode_T AppTelemetryPublish_Setup(const AppRuntimeConfig_T  * configPtr) {

	assert(configPtr);
	assert(configPtr->targetTelemetryConfigPtr);
	assert(configPtr->topicConfigPtr);
	assert(configPtr->activeTelemetryRTParamsPtr);

	Retcode_T retcode = RETCODE_OK;

	if(RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_targetTelemetryConfig, configPtr->targetTelemetryConfigPtr);

	if(RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_topicConfig, configPtr->topicConfigPtr);

	if(RETCODE_OK == retcode) retcode = AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, configPtr->activeTelemetryRTParamsPtr);

	return retcode;
}
/**
 * @brief Applies a new runtime config to the module.
 *
 * @details Extracts the following, depending on configElement
 * @details AppRuntimeConfig_Element_targetTelemetryConfig: qos
 * @details AppRuntimeConfig_Element_topicConfig: creates the new topic
 * @details AppRuntimeConfig_Element_activeTelemetryRTParams: the publishing frequency
 *
 * @note Call only if publishing task is not running.
 *
 * @param[in] configElement : the type of config to apply.
 * @param[in] newConfigPtr : the pointer, must match the type. Note: no type checking performed.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME)
 */
Retcode_T AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr) {

	assert(newConfigPtr);

	assert(appTelemetryPublish_TaskHandle==NULL);

	Retcode_T retcode = RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appTelemetryPublish_TaskSemaphoreHandle, APP_TELEMETRY_PUBLISHING_TASK_DELETE_INTERNAL_WAIT_TICKS)) {

		switch(configElement) {
		case AppRuntimeConfig_Element_targetTelemetryConfig: {
			appTelemetryPublish_MqttPublishInfo.qos = ((AppRuntimeConfig_TelemetryConfig_T *) newConfigPtr)->received.qos;
		}
		break;
		case AppRuntimeConfig_Element_topicConfig: {

			if(appTelemetryPublish_MqttPublishInfo.topic != NULL) free(appTelemetryPublish_MqttPublishInfo.topic);

			appTelemetryPublish_MqttPublishInfo.topic = AppMisc_FormatTopic("%s/iot-event/%s/%s/metrics",
														((AppRuntimeConfig_TopicConfig_T * ) newConfigPtr)->received.methodCreate,
														((AppRuntimeConfig_TopicConfig_T * ) newConfigPtr)->received.baseTopic,
														appTelemetryPublish_DeviceId);
		}
		break;
		case AppRuntimeConfig_Element_activeTelemetryRTParams:
			appTelemetryPublish_publishPeriodcityMillis = ((AppRuntimeConfig_TelemetryRTParams_T *) newConfigPtr)->publishPeriodcityMillis;
			break;
		default: retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);
		}

		xSemaphoreGive(appTelemetryPublish_TaskSemaphoreHandle);

	} else return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME);

	return retcode;
}
/**
 * @brief Create the publishing task.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_TASK)
 */
Retcode_T AppTelemetryPublish_CreatePublishingTask(void) {

	Retcode_T retcode = RETCODE_OK;

	assert(appTelemetryPublish_TaskHandle==NULL);

	if (pdPASS != xTaskCreate(	appTelemetryPublishing_TelemetryPublishTask,
								(const char* const ) "PublishTask",
								appTelemetryPublish_PublishTaskStackSize,
								NULL,
								appTelemetryPublish_PublishTaskPriority,
								&appTelemetryPublish_TaskHandle)) {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_TASK);
	}
	return retcode;
}
/**
 * @brief Delete the publishing task. Does nothing if task is not running.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME)
 */
Retcode_T AppTelemetryPublish_DeletePublishingTask(void) {

	Retcode_T retcode = RETCODE_OK;

	if(appTelemetryPublish_TaskHandle==NULL) return RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appTelemetryPublish_TaskSemaphoreHandle, APP_TELEMETRY_PUBLISHING_TASK_DELETE_INTERNAL_WAIT_TICKS)) {
		vTaskDelete(appTelemetryPublish_TaskHandle);
		appTelemetryPublish_TaskHandle = NULL;
		xSemaphoreGive(appTelemetryPublish_TaskSemaphoreHandle);
	} else {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME);
	}
	return retcode;
}
/**
 * @brief Check if publishing task is running.
 *
 * @return bool : Returns true if task is running, false otherwise.
 */
bool AppTelemetryPublish_isTaskRunning(void) {

	bool isRunning = false;
	if(pdTRUE == xSemaphoreTake(appTelemetryPublish_TaskSemaphoreHandle, APP_TELEMETRY_PUBLISHING_TASK_DELETE_INTERNAL_WAIT_TICKS)) {
		isRunning = (appTelemetryPublish_TaskHandle!=NULL);
		xSemaphoreGive(appTelemetryPublish_TaskSemaphoreHandle);
	} else assert(0);
	return isRunning;
}
/**
 * @brief The publishing task. Waits for a full queue for #appTelemetryPublish_publishPeriodcityMillis millis and publishes all payloads in the queue.
 * Keeps track in the stats of slow publishing loops.
 */
static void appTelemetryPublishing_TelemetryPublishTask(void* pvParameters) {
	BCDS_UNUSED(pvParameters);

	#ifdef DEBUG_APP_TELEMETRY_PUBLISH
	printf("[INFO] - appTelemetryPublishing_TelemetryPublishTask: starting ...\r\n");
	printf("[INFO] - appTelemetryPublishing_TelemetryPublishTask: publishPeriodcityMillis:%lu\r\n", appTelemetryPublish_publishPeriodcityMillis);
	printf("[INFO] - appTelemetryPublishing_TelemetryPublishTask: qos:%lu\r\n", appTelemetryPublish_MqttPublishInfo.qos);
	#endif

    char * payloadStr = NULL;

    // counters for measuring publish times
    TickType_t loopStartTicks = 0;
    uint32_t loopDurationTicks = 0;

	while (1) {

		if(pdTRUE == xSemaphoreTake(appTelemetryPublish_TaskSemaphoreHandle, APP_TELEMETRY_PUBLISHING_TASK_INTERNAL_WAIT_TICKS)) {

			// measure the publishing time
			loopStartTicks = xTaskGetTickCount();

			/**
			 * Wait for the full queue for one cycle target time
			 */
			if( RETCODE_OK != AppTelemetryQueue_Wait4FullQueue(appTelemetryPublish_publishPeriodcityMillis) ) {
				/*
				 * Observed:
				 * - when sampling has been suspended for changing frequency. happens 1 time. ok, don't do anything
				 * - when button (ISR) takes too much time from sampling - press button constantly
				 * - when sending recurring status message (e.g. full status) with qos=1
				 */
				AppStatus_Stats_IncrementTelemetrySendFailedCounter();

			} else {

		    	payloadStr = AppTelemetryQueue_RetrieveData();
		    	assert(payloadStr != NULL);

				appTelemetryPublish_MqttPublishInfo.payload = payloadStr;
				appTelemetryPublish_MqttPublishInfo.payloadLength = strlen(payloadStr);

				#ifdef DEBUG_APP_TELEMETRY_PUBLISH_EVERY_MESSAGE
				printf("[INFO] - appTelemetryPublishing_TelemetryPublishTask: publishing:\r\n");
				printf("\ttopic:%s, qos=%lu\r\n", appTelemetryPublish_MqttPublishInfo.Topic, appTelemetryPublish_MqttPublishInfo.QoS);
				printf("\tpayload:%s\r\n", appTelemetryPublish_MqttPublishInfo.Payload);
				printf("\tpayload length:%lu\r\n", appTelemetryPublish_MqttPublishInfo.PayloadLength);
				#endif

				Retcode_T retcode = AppMqtt_Publish(&appTelemetryPublish_MqttPublishInfo);

				if(RETCODE_OK != retcode) AppStatus_Stats_IncrementTelemetrySendFailedCounter();

				free(payloadStr);

			} // full queue

			loopDurationTicks = (xTaskGetTickCount()-loopStartTicks);
			if(loopDurationTicks > appTelemetryPublish_publishPeriodcityMillis) {
				AppStatus_Stats_IncrementTelemetrySendTooSlowCounter();
			}

			xSemaphoreGive(appTelemetryPublish_TaskSemaphoreHandle);

		} // task semaphore
	}//while
}

/**@} */
/** ************************************************************************* */

