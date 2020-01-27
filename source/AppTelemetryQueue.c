/*
 * AppTelemetryQueue.c
 *
 *  Created on: 19 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppTelemetryQueue AppTelemetryQueue
 * @{
 *
 * @brief Implements the telemetry queue, which is a JSON array. The queue is filled by @ref AppTelemetrySampling and read by @ref AppTelemetryPublish.
 * @details Has an internal write queue and a read queue. When the write queue is full, the read queue pointer will point to the write
 * queue and the write queue is created again. At this point, a read-trigger semaphore is released to notify waiting publishers / readers of the queue that it is ready for reading.
 * @details Provides also functions to create a 'test' queue - to verify that a new configuration would result in a valid queue size.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_TELEMETRY_QUEUE

#include "AppTelemetryQueue.h"
#include "AppMisc.h"

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t appTelemetryQueue_ChangeSemaphoreHandle = NULL; /**< semaphore to change the queue configuration */
#define APP_TELEMETRY_QUEUE_CHANGE_INTERNAL_WAIT_TICKS			UINT32_C(2000) /**< wait ticks for changing configuration of the module*/

/**
 * @brief Block access to the queue to change its configuration.
 */
static bool appTelemetryQueue_BlockAccess(void) {
	if(pdTRUE != xSemaphoreTake(appTelemetryQueue_ChangeSemaphoreHandle, MILLISECONDS(APP_TELEMETRY_QUEUE_CHANGE_INTERNAL_WAIT_TICKS)) ) {
		return false;
	}
	return true;
}
/**
 * @brief Allow access to the queue.
 */
static void appTelemetryQueue_AllowAccess(void) {
	xSemaphoreGive(appTelemetryQueue_ChangeSemaphoreHandle);
}

// the write queue
static cJSON * appTelemetryQueue_WriteJsonHandle = NULL;		/**< the write queue handle */
static uint8_t appTelemetryQueue_WriteJsonArrayCurrentSize = 0;	/**< the current size of the write queue */
static SemaphoreHandle_t appTelemetryQueue_WriteSemaphoreHandle = NULL; /**< semaphore for the write queue */
// the read queue
static cJSON * appTelemetryQueue_ReadJsonHandle = NULL; /**< the read queue handle */
static SemaphoreHandle_t appTelemetryQueue_ReadSemaphoreHandle = NULL; /**< the read queue semaphore */
#define APP_TELEMETRY_QUEUE_READ_INTERNAL_WAIT_TICKS			UINT32_C(10) /**< wait ticks to get the read semaphore */
// trigger for reading
static SemaphoreHandle_t appTelemetryQueue_ReadTriggerSemaphoreHandle = NULL; /**< semaphore to trigger reading / indicate the queue is full */

static uint8_t appTelemetryQueue_FullSize = 1; /**< variable for the full size of the telemetry queue */

/**
 * @brief Initialize the module.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 */
Retcode_T AppTelemetryQueue_Init(void) {

	Retcode_T retcode = RETCODE_OK;

	if(RETCODE_OK == retcode) {
		appTelemetryQueue_ChangeSemaphoreHandle = xSemaphoreCreateBinary();
		if(appTelemetryQueue_ChangeSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appTelemetryQueue_ChangeSemaphoreHandle);
	}

	if(RETCODE_OK == retcode) {
		appTelemetryQueue_WriteSemaphoreHandle = xSemaphoreCreateBinary();
		if(appTelemetryQueue_WriteSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appTelemetryQueue_WriteSemaphoreHandle);
	}
	if(RETCODE_OK == retcode) {
		appTelemetryQueue_ReadSemaphoreHandle = xSemaphoreCreateBinary();
		if(appTelemetryQueue_ReadSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appTelemetryQueue_ReadSemaphoreHandle);
	}
	if(RETCODE_OK == retcode) {
		appTelemetryQueue_ReadTriggerSemaphoreHandle = xSemaphoreCreateBinary();
		if(appTelemetryQueue_ReadTriggerSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
		xSemaphoreGive(appTelemetryQueue_ReadTriggerSemaphoreHandle);
	}

	return retcode;
}
/**
 * @brief Setup the module with the configuration.
 * @param[in] configPtr: the runtime configuration
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref AppTelemetryQueue_ApplyNewRuntimeConfig()
 */
Retcode_T AppTelemetryQueue_Setup(const AppRuntimeConfig_T * configPtr) {
	Retcode_T retcode = RETCODE_OK;

	assert(configPtr != NULL);
	assert(configPtr->activeTelemetryRTParamsPtr);

	if(RETCODE_OK == retcode) retcode = AppTelemetryQueue_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, configPtr->activeTelemetryRTParamsPtr);

	return retcode;
}
/**
 * @brief Prepare the telemetry queue. Creates the internal write and read JSON handles, releases the write / read semaphores respectively and blocks the read-trigger semaphore.
 * @param[in] queueSize: the number of samples in the queue before the read trigger semaphore is released. Saved in internal variable #appTelemetryQueue_FullSize
 */
static void appTelemetryQueue_Prepare(uint8_t queueSize) {

	assert(queueSize>0);

	// set ready to write
	if(NULL != appTelemetryQueue_WriteJsonHandle) cJSON_Delete(appTelemetryQueue_WriteJsonHandle);
	appTelemetryQueue_WriteJsonHandle = cJSON_CreateArray();
	appTelemetryQueue_WriteJsonArrayCurrentSize = 0;
	xSemaphoreGive(appTelemetryQueue_WriteSemaphoreHandle);

	// set ready to read
	if(NULL != appTelemetryQueue_ReadJsonHandle) cJSON_Delete(appTelemetryQueue_ReadJsonHandle);
	appTelemetryQueue_ReadJsonHandle = NULL;
	xSemaphoreGive(appTelemetryQueue_ReadSemaphoreHandle);
	//if(pdTRUE != xSemaphoreTake(appTelemetryQueue_ReadSemaphoreHandle, 0)) assert(0);

	// block the read trigger
	xSemaphoreGive(appTelemetryQueue_ReadTriggerSemaphoreHandle);
	if(pdTRUE != xSemaphoreTake(appTelemetryQueue_ReadTriggerSemaphoreHandle, 0)) assert(0);

	// set the size
	appTelemetryQueue_FullSize = queueSize;
}
/**
 * @brief External interface to prepare the telemetry queue.
 * @see appTelemetryQueue_Prepare()
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppTelemetryQueue_Prepare(void) {

	assert(appTelemetryQueue_BlockAccess());

	Retcode_T retcode = RETCODE_OK;

	appTelemetryQueue_Prepare(appTelemetryQueue_FullSize);

	appTelemetryQueue_AllowAccess();

	return retcode;

}
/**
 * @brief Apply a new runtime configuration to the module.
 * @param[in] configElement: the type of configuration to apply. Only #AppRuntimeConfig_Element_activeTelemetryRTParams is supported.
 * @param[in] newConfigPtr: the configuration of type configElement
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 */
Retcode_T AppTelemetryQueue_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr) {

	assert(newConfigPtr);

	if(configElement != AppRuntimeConfig_Element_activeTelemetryRTParams) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);

	assert(appTelemetryQueue_BlockAccess());

	Retcode_T retcode = RETCODE_OK;

	appTelemetryQueue_Prepare(((AppRuntimeConfig_TelemetryRTParams_T *) newConfigPtr)->numberOfSamplesPerEvent);

	appTelemetryQueue_AllowAccess();

	return retcode;
}
/**
 * @brief Add a sensor sample to the queue. Waits a max of waitTicks to obtain the write semaphore handle.
 *
 * @param[in] payloadPtr : the sample payload to add
 * @param[in] waitTicks : the ticks to wait to obtain the queue write semaphore handle
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_TELEMETRY_QUEUE_ALREADY_FULL) - indicates sync mismatch between reader and writer
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_TELEMETRY_QUEUE_CANT_TAKE_SEMAPHORE) when queue is blocked for writing
 *
 */
Retcode_T AppTelemetryQueue_AddSample(AppTelemetryPayload_T * payloadPtr, const uint32_t waitTicks) {

	assert(payloadPtr);

	assert(appTelemetryQueue_BlockAccess());

	Retcode_T retcode = RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appTelemetryQueue_WriteSemaphoreHandle, waitTicks)) {

		if ( appTelemetryQueue_WriteJsonArrayCurrentSize < appTelemetryQueue_FullSize ) {

			assert(appTelemetryQueue_WriteJsonHandle);

			// add the item
			cJSON_AddItemToArray(appTelemetryQueue_WriteJsonHandle, payloadPtr);
			appTelemetryQueue_WriteJsonArrayCurrentSize++;

			// is it full now? if so, change over
			if(appTelemetryQueue_WriteJsonArrayCurrentSize == appTelemetryQueue_FullSize) {

				// set the read handle
				if(pdTRUE == xSemaphoreTake(appTelemetryQueue_ReadSemaphoreHandle, APP_TELEMETRY_QUEUE_READ_INTERNAL_WAIT_TICKS)) {

					// the old one may not have been pickup up
					if(NULL != appTelemetryQueue_ReadJsonHandle) cJSON_Delete(appTelemetryQueue_ReadJsonHandle);

					appTelemetryQueue_ReadJsonHandle = appTelemetryQueue_WriteJsonHandle;

					xSemaphoreGive(appTelemetryQueue_ReadSemaphoreHandle);

					// trigger read
					// note: at config change it may be that last message was not read
					// hence, give semaphore without checking
					//if(pdTRUE != xSemaphoreGive(appTelemetryQueueReadTriggerSemaphoreHandle)) assert(0);
					xSemaphoreGive(appTelemetryQueue_ReadTriggerSemaphoreHandle);

				} else {
					assert(0);
				}

				// create a new write handle
				appTelemetryQueue_WriteJsonHandle = cJSON_CreateArray();
				appTelemetryQueue_WriteJsonArrayCurrentSize = 0;

			}

		} else {
			// indicates sync mismatch between writer & reader
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_TELEMETRY_QUEUE_ALREADY_FULL);
		}

		// allow writing again
		xSemaphoreGive(appTelemetryQueue_WriteSemaphoreHandle);

	} else retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_TELEMETRY_QUEUE_CANT_TAKE_SEMAPHORE);

	appTelemetryQueue_AllowAccess();

	return retcode;
}
/**
 * @brief Wait for waitTicks for a full queue based on the read-trigger semaphore. Used by @ref AppTelemetryPublish
 * @param[in] waitTicks: the number of ticks to wait for the read-trigger semaphore to become available
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_TELEMETRY_QUEUE_CANT_TAKE_SEMAPHORE)
 */
Retcode_T AppTelemetryQueue_Wait4FullQueue(uint32_t waitTicks) {
	Retcode_T retcode = RETCODE_OK;

	if(pdTRUE != xSemaphoreTake(appTelemetryQueue_ReadTriggerSemaphoreHandle, waitTicks)) retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_TELEMETRY_QUEUE_CANT_TAKE_SEMAPHORE);

	return retcode;
}
/**
 * @brief Retrieve the data from a full queue as a string and delete the internal read queue. Used by @ref AppTelemetryPublish.
 * @return char *: the data string to publish
 */
char * AppTelemetryQueue_RetrieveData(void) {

	assert(appTelemetryQueue_BlockAccess());

	char * payloadStr = NULL;
	if(pdTRUE == xSemaphoreTake(appTelemetryQueue_ReadSemaphoreHandle, APP_TELEMETRY_QUEUE_READ_INTERNAL_WAIT_TICKS)) {

		if(appTelemetryQueue_ReadJsonHandle != NULL) {
			payloadStr = cJSON_PrintUnformatted(appTelemetryQueue_ReadJsonHandle);
			cJSON_Delete(appTelemetryQueue_ReadJsonHandle);
			appTelemetryQueue_ReadJsonHandle = NULL;
		}

		xSemaphoreGive(appTelemetryQueue_ReadSemaphoreHandle);

	} else assert(0);

	appTelemetryQueue_AllowAccess();

	return payloadStr;
}

/* TEST queue */

cJSON *TestTelemetryQueueJSONArray = NULL; /**< the test queue */
uint8_t TestTelemetryQueueSize = 1; /**< test queue size with default */
/**
 * @brief Create a new test queue.
 * @param[in] queueSize: the size / number of payload messages in the queue.
 */
void AppTelemetryQueueCreateNewTestQueue(uint8_t queueSize) {
	TestTelemetryQueueJSONArray = cJSON_CreateArray();
	TestTelemetryQueueSize = queueSize;
}
/**
 * @brief Add a sample / payload to the test queue.
 * @param[in] payloadPtr: the payload to add
 */
void AppTelemetryQueueTestQueueAddSample(AppTelemetryPayload_T * payloadPtr) {
	cJSON_AddItemToArray(TestTelemetryQueueJSONArray, payloadPtr);
}
/**
 * @brief Get the length / size of the raw data from the test queue.
 * @return uint32_t: the size in bytes.
 */
uint32_t AppTelemetryQueueTestQueueGetDataSize(void) {
	char *s = cJSON_PrintUnformatted(TestTelemetryQueueJSONArray);
	int32_t s_length = strlen(s);
	free(s);
	return s_length;
}
/**
 * @brief Delete the test queue.
 */
void AppTelemetryQueueTestQueueDelete(void) {
	cJSON_Delete(TestTelemetryQueueJSONArray);
	TestTelemetryQueueJSONArray = NULL;
}
/**
 * @brief Print the test queue on the console.
 */
void AppTelemetryQueueTestQueuePrint(void) {
	printJSON(TestTelemetryQueueJSONArray);
}

/**@} */
/** ************************************************************************* */
