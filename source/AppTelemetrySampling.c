/*
 * AppTelemetrySampling.c
 *
 *  Created on: 25 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppTelemetrySampling AppTelemetrySampling
 * @{
 *
 * @brief Module to take periodic sensor samples based on the configuration.
 *
 * @see AppTelemetryPayload
 * @see AppTelemetryQueue
 * @see AppTelemetryPublish
 *
 * @author $(SOLACE_APP_AUTHOR)
 *
 * @date $(SOLACE_APP_DATE)
 *
 * @file
 *
 **/
#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_TELEMETRY_SAMPLING

#include "AppTelemetrySampling.h"
#include "AppTelemetryPayload.h"
#include "AppTelemetryQueue.h"
#include "AppMisc.h"
#include "AppStatus.h"

#include "XDK_Sensor.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"


static xTaskHandle appTelemetrySampling_TaskHandle = NULL; /**< the samling task task handle */
static uint32_t appTelemetrySampling_SamplingTaskPriority = 1; /**< sampling task priority with default */
static uint32_t appTelemetrySampling_SamplingTaskStackSize = 1024; /**< sampling task stack size with default */
static SemaphoreHandle_t appTelemetrySampling_TaskSemaphoreHandle = NULL; /**< sampling task semaphore */
#define APP_TELEMETRY_SAMPLING_TASK_INTERNAL_WAIT_TICKS			UINT32_C(10) /**< wait ticks to start sampling loop */
#define APP_TELEMETRY_SAMPLING_TASK_DELETE_INTERNAL_WAIT_TICKS	UINT32_C(5000)	/**< wait ticks to delete sampling task */

static TickType_t appTelemetrySampling_SamplingPeriodicityMillis = 1000; /**< local copy of configuration for sampling interval in millis */
static char * appTelemetrySampling_DeviceId = NULL; /**< local copy of device id */


#define APP_TEMPERATURE_OFFSET_CORRECTION               (-3459)/**< Macro for static temperature offset correction. Self heating, temperature correction factor */
/**
 * @brief Sensor set-up structure.
 */
static Sensor_Setup_T appTelemetrySampling_SensorSetup = {
	.CmdProcessorHandle = NULL,
	.Enable = { .Accel = true, .Mag = true, .Gyro = true, .Humidity = true, .Temp = true, .Pressure = true, .Light = true, .Noise = false, },
	.Config = {
		.Accel = { .Type = SENSOR_ACCEL_BMI160, .IsRawData = false, .IsInteruptEnabled = false, .Callback = NULL, },
		.Gyro = { .Type = SENSOR_GYRO_BMI160, .IsRawData = false, },
		.Mag = { .IsRawData = false },
		.Light = { .IsInteruptEnabled = false, .Callback = NULL, },
		.Temp = { .OffsetCorrection = APP_TEMPERATURE_OFFSET_CORRECTION, },
	},
};

/* forward declarations */
static void appTelemetrySampling_TelemetrySamplingTask(void* pvParameters);


/**
 * @brief Initialize the sampling module. Call in @ref AppController_Init().
 *
 * @param[in] deviceId: the device id
 * @param[in] samplingTaskPriority: the priority of the sampling task
 * @param[in] samplingTaskStackSize: the stack size of the sampling task
 * @param[in] sensorProcessorHandle: the command processor handle for the sensor module
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 */
Retcode_T AppTelemetrySampling_Init(const char * deviceId, uint32_t samplingTaskPriority, uint32_t samplingTaskStackSize, const CmdProcessor_T * sensorProcessorHandle) {

	assert(deviceId);
	assert(sensorProcessorHandle);

	Retcode_T retcode = RETCODE_OK;

	appTelemetrySampling_DeviceId = copyString(deviceId);

	appTelemetrySampling_SamplingTaskPriority = samplingTaskPriority;

	appTelemetrySampling_SamplingTaskStackSize = samplingTaskStackSize;

	appTelemetrySampling_SensorSetup.CmdProcessorHandle = (CmdProcessor_T *) sensorProcessorHandle;

	appTelemetrySampling_TaskSemaphoreHandle = xSemaphoreCreateBinary();
	if(appTelemetrySampling_TaskSemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appTelemetrySampling_TaskSemaphoreHandle);

	return retcode;

}
/**
 * @brief Setup of the sampling module. Call in @ref AppController_Setup().
 * @param[in] configPtr: the runtime configuration. Only reads activeTelemetryRTParamsPtr.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from Sensor_Setup()
 * @return Retcode_T: retcode from @ref AppTelemetrySampling_ApplyNewRuntimeConfig()
 */
Retcode_T AppTelemetrySampling_Setup(const AppRuntimeConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->activeTelemetryRTParamsPtr);

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = Sensor_Setup(&appTelemetrySampling_SensorSetup);

	if (RETCODE_OK == retcode) retcode = AppTelemetrySampling_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_activeTelemetryRTParams, configPtr->activeTelemetryRTParamsPtr);

	return retcode;
}
/**
 * @brief Enable the module. Call in @ref AppController_Enable().
 * @return Retcode_T: retcode from Sensor_Enable()
 */
Retcode_T AppTelemetrySampling_Enable(void) {
	return Sensor_Enable();
}
/**
 * @brief Apply a new configuration. Call only when the sampling task is not running.
 * @param[in] configElement: the configuration element to apply. only #AppRuntimeConfig_Element_activeTelemetryRTParams is supported.
 * @param[in] newConfigPtr: the new configuration of type configElement
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 */
Retcode_T AppTelemetrySampling_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr) {

	assert(newConfigPtr);
	assert(appTelemetrySampling_TaskHandle==NULL);

	Retcode_T retcode = RETCODE_OK;

	switch(configElement) {
	case AppRuntimeConfig_Element_activeTelemetryRTParams:

		appTelemetrySampling_SamplingPeriodicityMillis = ((AppRuntimeConfig_TelemetryRTParams_T *) newConfigPtr)->samplingPeriodicityMillis;

		break;
	default: return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);
	}

	return retcode;
}
/**
 * @brief Create the sampling task. Call only if it isn't running.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_TASK)
 */
Retcode_T AppTelemetrySampling_CreateSamplingTask(void) {

	Retcode_T retcode = RETCODE_OK;

	assert(appTelemetrySampling_TaskHandle==NULL);

	if (pdPASS != xTaskCreate(	appTelemetrySampling_TelemetrySamplingTask,
								(const char* const ) "SamplingTask",
								appTelemetrySampling_SamplingTaskStackSize,
								NULL,
								appTelemetrySampling_SamplingTaskPriority,
								&appTelemetrySampling_TaskHandle)) {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_TASK);
	}
	return retcode;
}
/**
 * @brief Delete sampling task. Does nothing if it doesn't exist.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME) if it can't take the semaphore in #APP_TELEMETRY_SAMPLING_TASK_DELETE_INTERNAL_WAIT_TICKS ticks.
 */
Retcode_T AppTelemetrySampling_DeleteSamplingTask(void) {

	Retcode_T retcode = RETCODE_OK;

	if(appTelemetrySampling_TaskHandle==NULL) return RETCODE_OK;

	if(pdTRUE == xSemaphoreTake(appTelemetrySampling_TaskSemaphoreHandle, APP_TELEMETRY_SAMPLING_TASK_DELETE_INTERNAL_WAIT_TICKS)) {
		vTaskDelete(appTelemetrySampling_TaskHandle);
		appTelemetrySampling_TaskHandle = NULL;
		xSemaphoreGive(appTelemetrySampling_TaskSemaphoreHandle);
	} else {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME);
	}
	return retcode;
}
/**
 * @brief Check if sampling task is running.
 * @return bool: true if task is running, false it it is not running.
 */
bool AppTelemetrySampling_isTaskRunning(void) {
	bool isRunning = false;
	if(pdTRUE == xSemaphoreTake(appTelemetrySampling_TaskSemaphoreHandle, APP_TELEMETRY_SAMPLING_TASK_DELETE_INTERNAL_WAIT_TICKS)) {
		isRunning = (appTelemetrySampling_TaskHandle!=NULL);
		xSemaphoreGive(appTelemetrySampling_TaskSemaphoreHandle);
	} else assert(0);
	return isRunning;
}
/**
 * @brief The sampling task.
 * Runs a loop with a delay of the configured sampling interval. Reads the sensor data, creates a new @ref AppTelemetryPayload and adds it to the
 * @ref AppTelemetryQueue.
 * Updates the stats if sampling is slower than expected interval using @ref AppStatus_Stats_IncrementTelemetrySamplingTooSlowCounter().
 * @param[in] pvParameters: unused.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ERROR_READING_SENSOR_DATA)
 * @exception Retcode_RaiseError: retcode from @ref AppTelemetryQueue_AddSample()
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ADD_SAMPLE_TO_QUEUE))
 */
static void appTelemetrySampling_TelemetrySamplingTask(void* pvParameters) {
	BCDS_UNUSED(pvParameters);

	Retcode_T retcode = RETCODE_OK;
	Retcode_T retcode_addQueue = RETCODE_OK;

	Sensor_Value_T sensorValue;
    memset(&sensorValue, 0x00, sizeof(sensorValue));

    TickType_t startLoopTicks = 0;
    int32_t loopDelayTicks = 0;

    AppTelemetryPayload_T * payloadPtr = NULL;

    while (1) {

    	if(pdTRUE == xSemaphoreTake(appTelemetrySampling_TaskSemaphoreHandle, APP_TELEMETRY_SAMPLING_TASK_INTERNAL_WAIT_TICKS)) {

    		startLoopTicks = xTaskGetTickCount();

    		retcode = Sensor_GetData(&sensorValue);

    		if(RETCODE_OK != retcode) {
				// never observed
    			Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ERROR_READING_SENSOR_DATA));
				AppStatus_Stats_IncrementTelemetrySamplingTooSlowCounter();

			} else {

				payloadPtr = AppTelemetryPayload_CreateNew(startLoopTicks, &sensorValue);

				retcode_addQueue = AppTelemetryQueue_AddSample(payloadPtr, appTelemetrySampling_SamplingPeriodicityMillis);

				if(RETCODE_OK != retcode_addQueue) {
					// never observed
					Retcode_RaiseError(retcode_addQueue);
					Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ADD_SAMPLE_TO_QUEUE));
					AppTelemetryPayload_Delete(payloadPtr);
				}
			}

			//calculate delay and wait
			loopDelayTicks = appTelemetrySampling_SamplingPeriodicityMillis-(xTaskGetTickCount()-startLoopTicks);
			if(loopDelayTicks > 0) vTaskDelay((TickType_t) loopDelayTicks);
			else {
				AppStatus_Stats_IncrementTelemetrySamplingTooSlowCounter();
			}

			xSemaphoreGive(appTelemetrySampling_TaskSemaphoreHandle);
    	} // task semaphore

    } // while
}
/**@} */
/** ************************************************************************* */
