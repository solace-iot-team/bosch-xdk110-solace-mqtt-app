/*
 * AppTelemetryPayload.c
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppTelemetryPayload AppTelemetryPayload
 * @{
 *
 * @brief This module abstracts the telemetry payload implementation. It is used by @ref AppTelemetrySampling and @ref AppTelemetryPublishing.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_TELEMETRY_PAYLOAD

#include "AppTelemetryPayload.h"
#include "AppMisc.h"

/* copies of local configuration */
static const char * appTelemetryPayload_DeviceId = NULL; /**< local copy of the device id */
static AppRuntimeConfig_Telemetry_PayloadFormat_T appTelemetryPayload_PayloadFormat = APP_RT_CFG_DEFAULT_PAYLOAD_FORMAT; /**< local copy of payload format configuration */
static AppRuntimeConfig_Sensors_T * appTelemetryPayload_TargetTelemetrySensorsPtr = NULL; /**< local copy of sensors configuration */

/* forwards */
static AppTelemetryPayload_T * appTelemetryPayload_CreateNew_V1_Json_Verbose(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr);
static AppTelemetryPayload_T * appTelemetryPayload_CreateNew_V1_Json_Compact(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr);

/**
 * @brief Initialize the module.
 * @param[in] deviceId: the deviceId, module takes a local copy
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppTelemetryPayload_Init(const char * deviceId) {

	appTelemetryPayload_DeviceId = copyString(deviceId);

	return RETCODE_OK;
}
/**
 * @brief Setup the module. Applies the runtime configuration.
 * @param[in] configPtr: the runtime configuration. Reads #AppRuntimeConfig_Element_targetTelemetryConfig
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref AppTelemetryPayload_ApplyNewRuntimeConfig()
 */
Retcode_T AppTelemetryPayload_Setup(const AppRuntimeConfig_T * configPtr) {
	Retcode_T retcode = RETCODE_OK;

	assert(configPtr != NULL);
	assert(configPtr->targetTelemetryConfigPtr);

	if(RETCODE_OK == retcode) retcode = AppTelemetryPayload_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_targetTelemetryConfig, configPtr->targetTelemetryConfigPtr);

	return retcode;
}
/**
 * @brief Apply a new runtime configuration and takes local copies.
 * @param[in] configElement: the configuration element, only #AppRuntimeConfig_Element_targetTelemetryConfig is supported.
 * @param[in] newConfigPtr: the new configuration pointer of type configElement
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 */
Retcode_T AppTelemetryPayload_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr) {

	assert(newConfigPtr);

	if(configElement != AppRuntimeConfig_Element_targetTelemetryConfig) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);

	Retcode_T retcode = RETCODE_OK;

	AppRuntimeConfig_TelemetryConfig_T * configPtr = (AppRuntimeConfig_TelemetryConfig_T *) newConfigPtr;

	appTelemetryPayload_PayloadFormat = configPtr->received.payloadFormat;

	if(appTelemetryPayload_TargetTelemetrySensorsPtr) AppRuntimeConfig_DeleteSensors(appTelemetryPayload_TargetTelemetrySensorsPtr);

	appTelemetryPayload_TargetTelemetrySensorsPtr = AppRuntimeConfig_DuplicateSensors(&configPtr->received.sensors);

	return retcode;
}
/**
 * @brief Create a new payload structure in the format as configured previously.
 * @param[in] tickCount: the current tick count, will be converted into a timestamp string
 * @param[in] sensorValuePtr: the values of the sensors to populate the payload
 * @return AppTelemetryPayload_T *: the newly created telemetry payload structure
 */
AppTelemetryPayload_T * AppTelemetryPayload_CreateNew(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr) {

	switch(appTelemetryPayload_PayloadFormat) {
	case AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Verbose:
		return appTelemetryPayload_CreateNew_V1_Json_Verbose(tickCount, sensorValuePtr);
		break;
	case AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Compact:
		return appTelemetryPayload_CreateNew_V1_Json_Compact(tickCount, sensorValuePtr);
		break;
	default:
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_TELEMETRY_PAYLOAD_UNSUPPORTED_FORMAT));
		return NULL;
		break;
	}
}
/**
 * @brief Create a test payload based on a new configuration. Used to test whether new configuration is valid. Leaves module's configuration intact.
 * @param[in] tickCount: the current tick count
 * @param[in] sensorValuePtr: the sensor values
 * @param[in] sensorsConfigPtr: the new sensor configuration to be tested
 * @param[in] payloadFormat: the payload format to be tested
 * @return AppTelemetryPayload_T *: the newly created telemetry payload structure
 * @see AppTelemetryPayload_CreateNew()
 */
AppTelemetryPayload_T * AppTelemetryPayload_CreateNew_Test(
		const TickType_t tickCount,
		const Sensor_Value_T * sensorValuePtr,
		const AppRuntimeConfig_Sensors_T * sensorsConfigPtr,
		AppRuntimeConfig_Telemetry_PayloadFormat_T payloadFormat) {

	// remember old values
	AppRuntimeConfig_Sensors_T * orgSensorsConfigPtr = appTelemetryPayload_TargetTelemetrySensorsPtr;
	AppRuntimeConfig_Telemetry_PayloadFormat_T orgPayloadFormat = appTelemetryPayload_PayloadFormat;

	appTelemetryPayload_TargetTelemetrySensorsPtr = (AppRuntimeConfig_Sensors_T *) sensorsConfigPtr;
	appTelemetryPayload_PayloadFormat = payloadFormat;

	AppTelemetryPayload_T * testPayloadPtr = AppTelemetryPayload_CreateNew(tickCount, sensorValuePtr);

	// restore original values
	appTelemetryPayload_TargetTelemetrySensorsPtr = orgSensorsConfigPtr;
	appTelemetryPayload_PayloadFormat = orgPayloadFormat;

	return testPayloadPtr;
}
/**
 * @brief Create a 'V1 JSON Verbose' payload.
 * @param[in] tickCount: the current tick count. Is converted to a timestamp using @ref AppTimestamp.
 * @param[in] sensorValuePtr: the values of the sensor readings
 * @return AppTelemetryPayload_T *: the created payload
 */
static AppTelemetryPayload_T * appTelemetryPayload_CreateNew_V1_Json_Verbose(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr) {

	char * timestampStr = AppTimestamp_CreateTimestampStr(AppTimestamp_GetTimestamp(tickCount));

	cJSON *sampleJSON = cJSON_CreateObject();

	cJSON_AddItemToObject(sampleJSON, "timestamp", cJSON_CreateString(timestampStr));
	free(timestampStr);

	cJSON_AddItemToObject(sampleJSON, "deviceId", cJSON_CreateString(appTelemetryPayload_DeviceId));

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isHumidity) cJSON_AddNumberToObject(sampleJSON, "humidity", (long int ) sensorValuePtr->RH);

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isLight) cJSON_AddNumberToObject(sampleJSON, "light", (long int ) sensorValuePtr->Light);

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isTemperature) cJSON_AddNumberToObject(sampleJSON, "temperature", (sensorValuePtr->Temp / 1000));

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isAccelerator) {
		cJSON_AddNumberToObject(sampleJSON, "acceleratorX", sensorValuePtr->Accel.X);
		cJSON_AddNumberToObject(sampleJSON, "acceleratorY", sensorValuePtr->Accel.Y);
		cJSON_AddNumberToObject(sampleJSON, "acceleratorZ", sensorValuePtr->Accel.Z);
	}
	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isGyro) {
		cJSON_AddNumberToObject(sampleJSON, "gyroX", sensorValuePtr->Gyro.X);
		cJSON_AddNumberToObject(sampleJSON, "gyroY", sensorValuePtr->Gyro.Y);
		cJSON_AddNumberToObject(sampleJSON, "gyroZ", sensorValuePtr->Gyro.Z);
	}
	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isMagneto) {
		cJSON_AddNumberToObject(sampleJSON, "magR", sensorValuePtr->Mag.R);
		cJSON_AddNumberToObject(sampleJSON, "magX", sensorValuePtr->Mag.X);
		cJSON_AddNumberToObject(sampleJSON, "magY", sensorValuePtr->Mag.Y);
		cJSON_AddNumberToObject(sampleJSON, "magZ", sensorValuePtr->Mag.Z);
	}

	return (AppTelemetryPayload_T *) sampleJSON;
}
/**
 * @brief Create a 'V1 JSON Compact' payload.
 * @param[in] tickCount: the current tick count. Is converted to a timestamp using @ref AppTimestamp.
 * @param[in] sensorValuePtr: the values of the sensor readings
 * @return AppTelemetryPayload_T *: the created payload
 */
static AppTelemetryPayload_T * appTelemetryPayload_CreateNew_V1_Json_Compact(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr) {

	char * timestampStr = AppTimestamp_CreateTimestampStr(AppTimestamp_GetTimestamp(tickCount));

	cJSON *sampleJSON = cJSON_CreateObject();

	cJSON_AddItemToObject(sampleJSON, "ts", cJSON_CreateString(timestampStr));
	free(timestampStr);

	cJSON_AddItemToObject(sampleJSON, "id", cJSON_CreateString(appTelemetryPayload_DeviceId));

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isHumidity) cJSON_AddNumberToObject(sampleJSON, "h", (long int ) sensorValuePtr->RH);

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isLight) cJSON_AddNumberToObject(sampleJSON, "l", (long int ) sensorValuePtr->Light);

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isTemperature) cJSON_AddNumberToObject(sampleJSON, "t", (sensorValuePtr->Temp / 1000));

	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isAccelerator) {
		cJSON_AddNumberToObject(sampleJSON, "aX", sensorValuePtr->Accel.X);
		cJSON_AddNumberToObject(sampleJSON, "aY", sensorValuePtr->Accel.Y);
		cJSON_AddNumberToObject(sampleJSON, "aZ", sensorValuePtr->Accel.Z);
	}
	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isGyro) {
		cJSON_AddNumberToObject(sampleJSON, "gX", sensorValuePtr->Gyro.X);
		cJSON_AddNumberToObject(sampleJSON, "gY", sensorValuePtr->Gyro.Y);
		cJSON_AddNumberToObject(sampleJSON, "gZ", sensorValuePtr->Gyro.Z);
	}
	if (appTelemetryPayload_TargetTelemetrySensorsPtr->isMagneto) {
		cJSON_AddNumberToObject(sampleJSON, "mR", sensorValuePtr->Mag.R);
		cJSON_AddNumberToObject(sampleJSON, "mX", sensorValuePtr->Mag.X);
		cJSON_AddNumberToObject(sampleJSON, "mY", sensorValuePtr->Mag.Y);
		cJSON_AddNumberToObject(sampleJSON, "mZ", sensorValuePtr->Mag.Z);
	}

	return (AppTelemetryPayload_T *) sampleJSON;
}
/**
 * @brief Delete a payload.
 * @param[in] payloadPtr: the payload to delete
 */
void AppTelemetryPayload_Delete(AppTelemetryPayload_T * payloadPtr) {
	cJSON_Delete(payloadPtr);
}

/**@} */
/** ************************************************************************* */




