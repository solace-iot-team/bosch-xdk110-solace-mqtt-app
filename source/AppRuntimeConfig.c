/*
 * AppRuntimeConfig.c
 *
 *  Created on: 23 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppRuntimeConfig AppRuntimeConfig
 * @{
 *
 * @brief Manages the runtime configuration for the App.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_RUNTIME_CONFIG

#include "AppRuntimeConfig.h"
#include "AppStatus.h"
#include "AppTelemetryPayload.h"
#include "AppTelemetryQueue.h"
#include "AppMisc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include "XDK_Sensor.h"
#include "XDK_Storage.h"
#include "BCDS_BSP_Board.h"

static bool appRuntimeConfig_isEnabled = false; /**< flag to indicate if module has been enabled by AppRuntimeConfig_Enable() */
static AppRuntimeConfig_T * appRuntimeConfigPtr = NULL; /**< the currently active runtime config */
static SemaphoreHandle_t appRuntimeConfigPtr_SemaphoreHandle = NULL; /**< semaphore to protect access to the config pointer */
#define APP_RUNTIME_CONFIG_PTR_TAKE_SEMAPHORE_WAIT_IN_MS			UINT32_C(0) /**< wait in millis to get access to the semaphore */

/**
 * @brief Default values if no runtime config exists yet.
 */
static const AppRuntimeConfig_TelemetryConfig_T appRuntimeConfig_DefaultTargetTelemetryConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
		.delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
        .activateAtBootTime = APP_RT_CFG_DEFAULT_ACTIVATE_AT_BOOT_TIME,
		.sensorEnableFlag = APP_RT_CFG_DEFAULT_SENSOR_ENABLE,
		.numberOfEventsPerSecond = APP_RT_CFG_DEFAULT_NUM_EVENTS_PER_SEC,
		.numberOfSamplesPerEvent = APP_RT_CFG_DEFAULT_NUM_SAMPLES_PER_EVENT,
	    .qos = APP_RT_CFG_DEFAULT_QOS,
	    .payloadFormat = APP_RT_CFG_DEFAULT_PAYLOAD_FORMAT,
		.sensors = {
			.isLight = true,
			.isAccelerator = true,
			.isGyro = true,
			.isMagneto = true,
			.isHumidity = true,
			.isTemperature = true,
			.isPressure = true,
		},
	},
};
/**
 * @brief Default values if no runtime config exists yet.
 */
static const AppRuntimeConfig_StatusConfig_T appRuntimeConfig_DefaultStatusConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
		.delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.isSendPeriodicStatus = APP_RT_CFG_DEFAULT_STATUS_SEND_PERIODIC_FLAG,
		.periodicStatusType = APP_RT_CFG_DEFAULT_STATUS_PERIODIC_TYPE,
		.periodicStatusIntervalSecs = APP_RT_CFG_DEFAULT_STATUS_INTERVAL_SECS,
		.qos=APP_RT_CFG_DEFAULT_STATUS_QOS
	},
};
/**
 * @brief Default values if no runtime config exists yet.
 */
static const AppRuntimeConfig_MqttBrokerConnectionConfig_T appRuntimeConfig_DefaultMqttBrokerConnectionConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
		.delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.brokerUrl = NULL,
		.brokerPort = UINT16_C(0),
		.brokerUsername = NULL,
		.brokerPassword = NULL,
		.isCleanSession = false,
		.keepAliveIntervalSecs = 60,
		.isSecureConnection = false,
	}
};
/**
 * @brief Default values if no runtime config exists yet.
 */
static const AppRuntimeConfig_TopicConfig_T appRuntimeConfig_DefaultTopicConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
		.delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.baseTopic = NULL,
		.methodCreate = NULL,
		.methodUpdate = NULL,
	}
};
/**
 * @brief Default values if no runtime config exists yet.
 */
static const AppRuntimeConfig_T appRuntimeConfig_DefaultAppRuntimeConfig = {
	.targetTelemetryConfigPtr = NULL,
	.activeTelemetryRTParamsPtr = NULL,
	.statusConfigPtr = NULL,
	.mqttBrokerConnectionConfigPtr = NULL,
	.topicConfigPtr = NULL,
	.internalState = {
		.source = AppRuntimeConfig_ConfigSource_InternalDefaults,
	}
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_TelemetryConfig_T appRuntimeConfig_EmptyTargetTelemetryConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
        .delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
        .activateAtBootTime = APP_RT_CFG_DEFAULT_ACTIVATE_AT_BOOT_TIME,
		.sensorEnableFlag = APP_RT_CFG_DEFAULT_SENSOR_ENABLE,
		.numberOfEventsPerSecond = 0,
		.numberOfSamplesPerEvent = 0,
	    .qos = 0,
	    .payloadFormat = AppRuntimeConfig_Telemetry_PayloadFormat_NULL,
		.sensors = {
			.isLight = false,
			.isAccelerator = false,
			.isGyro = false,
			.isMagneto = false,
			.isHumidity = false,
			.isTemperature = false,
			.isPressure = false,
		},
	},
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_TelemetryRTParams_T appRuntimeConfig_EmptyActiveTelemetryRTParams = {
	.samplingPeriodicityMillis = 0,
	.publishPeriodcityMillis = 0,
	.numberOfSamplesPerEvent = 0,
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_StatusConfig_T appRuntimeConfig_EmptyStatusConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
        .delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.isSendPeriodicStatus = APP_RT_CFG_DEFAULT_STATUS_SEND_PERIODIC_FLAG,
		.periodicStatusType = APP_RT_CFG_DEFAULT_STATUS_PERIODIC_TYPE,
		.periodicStatusIntervalSecs = APP_RT_CFG_DEFAULT_STATUS_INTERVAL_SECS,
		.qos=APP_RT_CFG_DEFAULT_STATUS_QOS
	}
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_MqttBrokerConnectionConfig_T appRuntimeConfig_EmptyMqttBrokerConnectionConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
        .delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.brokerUrl = NULL,
		.brokerPort = UINT16_C(0),
		.brokerUsername = NULL,
		.brokerPassword = NULL,
		.isCleanSession = false,
		.keepAliveIntervalSecs = 60,
		.isSecureConnection = false,
	}
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_TopicConfig_T appRuntimeConfig_EmptyTopicConfig = {
	.received = {
		.timestampStr = NULL,
		.exchangeIdStr = NULL,
		.tagsJsonHandle = NULL,
        .delay2ApplyConfigSeconds = APP_RT_CFG_DEFAULT_DELAY_SECS,
		.applyFlag = APP_RT_CFG_DEFAULT_APPLY,
		.baseTopic = NULL,
		.methodCreate = NULL,
		.methodUpdate = NULL,
	}
};
/**
 * @brief Empty values.
 */
static const AppRuntimeConfig_T appRuntimeConfig_EmptyAppRuntimeConfig = {
	.targetTelemetryConfigPtr = NULL,
	.activeTelemetryRTParamsPtr = NULL,
	.statusConfigPtr = NULL,
	.mqttBrokerConnectionConfigPtr = NULL,
	.topicConfigPtr = NULL,
	.internalState = {
		.source = AppRuntimeConfig_ConfigSource_ExternallySet,
	}
};

#define RT_CFG_FILE_NAME					"/runtime_config.json" /**< runtime config filename on the SD card */
#define RT_NEW_CFG_FILE_NAME				"/runtime_config_new.json" /**< the new config file name when updating the config */
#define RT_CFG_FILE_BUFFER_SIZE		UINT32_C(2048)	/**< the buffer size for the entire config file */

static uint8_t appRuntimeConfig_FileWriteBuffer[RT_CFG_FILE_BUFFER_SIZE] = { 0 }; /**< the actual write buffer */
/**
 * @brief Structure for writing the config file.
 */
static Storage_Write_T appRuntimeConfig_StorageWrite = {
	.FileName = RT_NEW_CFG_FILE_NAME,
	.WriteBuffer = appRuntimeConfig_FileWriteBuffer,
	.BytesToWrite = sizeof(appRuntimeConfig_FileWriteBuffer),
	.ActualBytesWritten = 0UL,
	.Offset = 0UL
};
/**
 * @brief Structure for renaming the config file when updating the config.
 */
static Storage_Rename_T appRuntimeConfig_StorageRenameInfo = {
		.OriginalFileName = RT_NEW_CFG_FILE_NAME,
		.NewFileName = RT_CFG_FILE_NAME
};

static char * appRuntimeConfig_DeviceId = NULL; /**< the internal device id */

/* forwards */
static AppRuntimeConfigStatus_T * appRuntimeConfig_CalculateAndValidateTelemetryRTParams(uint8_t numberOfSamplesPerEvent, uint8_t numberOfEventsPerSecond, AppRuntimeConfig_TelemetryRTParams_T * rtParamsPtr);

static cJSON * appRuntimeConfig_GetStatusAsJsonObject(const AppRuntimeConfigStatus_T * statusPtr);

static void appRuntimeConfig_DeleteTopicConfig(AppRuntimeConfig_TopicConfig_T * configPtr);

static void appRuntimeConfig_DeleteMqttBrokerConnectionConfig(AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr);

#if defined(DEBUG_APP_CONTROLLER) || defined(DEBUG_APP_RUNTIME_CONFIG)
static void appRuntimeConfig_Print(AppRuntimeConfig_ConfigElement_T configElement, const void * configPtr);
#endif


/* quick access functions */

/**
 * @brief Public function to get the runtime config. Checks if config is currently blocked and raise a fatal exception if it is.
 * @return AppRuntimeConfig_T *: the config
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_RT_CONFIG_ACCESS_TO_INTERNAL_PTR_BLOCKED)
 */
const AppRuntimeConfig_T * getAppRuntimeConfigPtr(void) {
	if(pdTRUE != xSemaphoreTake(appRuntimeConfigPtr_SemaphoreHandle, MILLISECONDS(APP_RUNTIME_CONFIG_PTR_TAKE_SEMAPHORE_WAIT_IN_MS)) ) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_RT_CONFIG_ACCESS_TO_INTERNAL_PTR_BLOCKED));
		return NULL;
	}
	xSemaphoreGive(appRuntimeConfigPtr_SemaphoreHandle);
	return appRuntimeConfigPtr;
}
/**
 * @brief Internal function to block access to the config.
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_RT_CONFIG_ACCESS_TO_INTERNAL_PTR_BLOCKED)
 */
static void appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr(void) {
	if(pdTRUE != xSemaphoreTake(appRuntimeConfigPtr_SemaphoreHandle, MILLISECONDS(APP_RUNTIME_CONFIG_PTR_TAKE_SEMAPHORE_WAIT_IN_MS)) ) {
		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_RT_CONFIG_ACCESS_TO_INTERNAL_PTR_BLOCKED));
	}
}
/**
 * @brief Internal function to allow access to the config.
 */
static void appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr(void) {
	xSemaphoreGive(appRuntimeConfigPtr_SemaphoreHandle);
}
/**
 * @brief Returns the JSON for the applyFlag.
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetApplyFlagAsJsonString(AppRuntimeConfig_Apply_T applyFlag) {
	if(AppRuntimeConfig_Apply_Transient == applyFlag) return cJSON_CreateString(APP_RT_CFG_APPLY_TRANSIENT);
	else if(AppRuntimeConfig_Apply_Persistent == applyFlag) return cJSON_CreateString(APP_RT_CFG_APPLY_PERSISTENT);
	else assert(0);
	return NULL;
}
/**
 * @brief Returns the JSON for the telemetry config.
 * @param[in] configPtr: the telemetry config pointer
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetTargetTelemetryConfigAsJsonObject(const AppRuntimeConfig_TelemetryConfig_T * configPtr) {

	assert(configPtr != NULL);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON * receivedJsonHandle = cJSON_CreateObject();

	cJSON_AddItemToObject(receivedJsonHandle, "timestamp", cJSON_CreateString(configPtr->received.timestampStr));

	cJSON_AddItemToObject(receivedJsonHandle, "exchangeId", cJSON_CreateString(configPtr->received.exchangeIdStr));

	cJSON_AddItemToObject(receivedJsonHandle, "tags", cJSON_Duplicate(configPtr->received.tagsJsonHandle,true));

	cJSON_AddNumberToObject(receivedJsonHandle, "delay", configPtr->received.delay2ApplyConfigSeconds);

	cJSON_AddItemToObject(receivedJsonHandle, "apply", appRuntimeConfig_GetApplyFlagAsJsonString(configPtr->received.applyFlag));

	cJSON_AddBoolToObject(receivedJsonHandle, "activateAtBootTime", configPtr->received.activateAtBootTime);

	if(AppRuntimeConfig_SensorEnable_All == configPtr->received.sensorEnableFlag)
		cJSON_AddItemToObject(receivedJsonHandle, "sensorsEnable", cJSON_CreateString(APP_RT_CFG_SENSORS_ENABLE_ALL));
	else if(AppRuntimeConfig_SensorEnable_SelectedOnly == configPtr->received.sensorEnableFlag)
		cJSON_AddItemToObject(receivedJsonHandle, "sensorsEnable", cJSON_CreateString(APP_RT_CFG_SENSORS_ENABLE_SELECTED));
	else assert(0);

	cJSON_AddNumberToObject(receivedJsonHandle, "eventFrequencyPerSec", configPtr->received.numberOfEventsPerSecond);

	cJSON_AddNumberToObject(receivedJsonHandle, "samplesPerEvent", configPtr->received.numberOfSamplesPerEvent);

	cJSON_AddNumberToObject(receivedJsonHandle, "qos", configPtr->received.qos);

	if(AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Verbose == configPtr->received.payloadFormat) {
		cJSON_AddItemToObject(receivedJsonHandle, "payloadFormat", cJSON_CreateString(APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR));
	} else if(AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Compact == configPtr->received.payloadFormat) {
		cJSON_AddItemToObject(receivedJsonHandle, "payloadFormat", cJSON_CreateString(APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_COMPACT_STR));
	} else assert(0);

	cJSON * sensorsJsonArrayHandle = cJSON_CreateArray();
	if(configPtr->received.sensors.isLight) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("light"));
	if(configPtr->received.sensors.isAccelerator) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("accelerator"));
	if(configPtr->received.sensors.isGyro) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("gyroscope"));
	if(configPtr->received.sensors.isMagneto) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("magnetometer"));
	if(configPtr->received.sensors.isHumidity) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("humidity"));
	if(configPtr->received.sensors.isTemperature) cJSON_AddItemToArray(sensorsJsonArrayHandle, cJSON_CreateString("temperature"));
	cJSON_AddItemToObject(receivedJsonHandle, "sensors", sensorsJsonArrayHandle);

	cJSON_AddItemToObject(jsonHandle, "received",receivedJsonHandle);

	return jsonHandle;

}
/**
 * @brief Returns the active runtime telemetry parameters config as JSON.
 * @param[in] configPtr: the runtime telemetry parameters config
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetActiveTelemetryRTParamsAsJsonObject(const AppRuntimeConfig_TelemetryRTParams_T * configPtr) {

	assert(configPtr != NULL);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON_AddNumberToObject(jsonHandle, "numberOfSamplesPerEvent", configPtr->numberOfSamplesPerEvent);
	cJSON_AddNumberToObject(jsonHandle, "publishPeriodcityMillis", configPtr->publishPeriodcityMillis);
	cJSON_AddNumberToObject(jsonHandle, "samplingPeriodicityMillis", configPtr->samplingPeriodicityMillis);

	return jsonHandle;
}
/**
 * @brief Returns the status config as JSON.
 * @param[in] configPtr: the status config
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetStatusConfigAsJsonObject(const AppRuntimeConfig_StatusConfig_T * configPtr) {

	assert(configPtr);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON * receivedJsonHandle = cJSON_CreateObject();
	cJSON_AddItemToObject(receivedJsonHandle, "timestamp", cJSON_CreateString(configPtr->received.timestampStr));
	cJSON_AddItemToObject(receivedJsonHandle, "exchangeId", cJSON_CreateString(configPtr->received.exchangeIdStr));
	cJSON_AddItemToObject(receivedJsonHandle, "tags", cJSON_Duplicate(configPtr->received.tagsJsonHandle,true));
	cJSON_AddNumberToObject(receivedJsonHandle, "delay", configPtr->received.delay2ApplyConfigSeconds);
	cJSON_AddItemToObject(receivedJsonHandle, "apply", appRuntimeConfig_GetApplyFlagAsJsonString(configPtr->received.applyFlag));

	cJSON_AddBoolToObject(receivedJsonHandle, "sendPeriodicStatus", configPtr->received.isSendPeriodicStatus);
	cJSON_AddNumberToObject(receivedJsonHandle, "periodicStatusIntervalSecs", configPtr->received.periodicStatusIntervalSecs);
	if(AppRuntimeConfig_PeriodicStatusType_Full == configPtr->received.periodicStatusType) {
		cJSON_AddItemToObject(receivedJsonHandle, "periodicStatusType", cJSON_CreateString(APP_RT_CFG_STATUS_PERIODIC_TYPE_FULL));
	} else if(AppRuntimeConfig_PeriodicStatusType_Short == configPtr->received.periodicStatusType) {
		cJSON_AddItemToObject(receivedJsonHandle, "periodicStatusType", cJSON_CreateString(APP_RT_CFG_STATUS_PERIODIC_TYPE_SHORT));
	} else assert(0);

	cJSON_AddNumberToObject(receivedJsonHandle, "qos", configPtr->received.qos);

	cJSON_AddItemToObject(jsonHandle, "received", receivedJsonHandle);

	return jsonHandle;

}
/**
 * @brief Returns the broker config as JSON.
 * @param[in] configPtr: the broker config
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetMqttBrokerConnectionConfigAsJsonObject(const AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr) {

	assert(configPtr != NULL);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON * receivedJsonHandle = cJSON_CreateObject();
	cJSON_AddItemToObject(receivedJsonHandle, "timestamp", cJSON_CreateString(configPtr->received.timestampStr));
	cJSON_AddItemToObject(receivedJsonHandle, "exchangeId", cJSON_CreateString(configPtr->received.exchangeIdStr));
	cJSON_AddItemToObject(receivedJsonHandle, "tags", cJSON_Duplicate(configPtr->received.tagsJsonHandle,true));
	cJSON_AddNumberToObject(receivedJsonHandle, "delay", configPtr->received.delay2ApplyConfigSeconds);
	cJSON_AddItemToObject(receivedJsonHandle, "apply", appRuntimeConfig_GetApplyFlagAsJsonString(configPtr->received.applyFlag));

	cJSON_AddItemToObject(receivedJsonHandle, "brokerURL", cJSON_CreateString(configPtr->received.brokerUrl));
	cJSON_AddNumberToObject(receivedJsonHandle, "brokerPort", configPtr->received.brokerPort);
	cJSON_AddItemToObject(receivedJsonHandle, "brokerUsername", cJSON_CreateString(configPtr->received.brokerUsername));
	cJSON_AddItemToObject(receivedJsonHandle, "brokerPassword", cJSON_CreateString(configPtr->received.brokerPassword));
	cJSON_AddBoolToObject(receivedJsonHandle, "cleanSession", configPtr->received.isCleanSession);
	cJSON_AddBoolToObject(receivedJsonHandle, "secureConnection", configPtr->received.isSecureConnection);
	cJSON_AddNumberToObject(receivedJsonHandle, "keepAliveIntervalSecs", configPtr->received.keepAliveIntervalSecs);

	cJSON_AddItemToObject(jsonHandle, "received", receivedJsonHandle);

	return jsonHandle;

}
/**
 * @brief Returns the topic config as JSON.
 * @param[in] configPtr: the topic config
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetTopicConfigAsJsonObject(const AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(configPtr != NULL);

	cJSON * jsonHandle = cJSON_CreateObject();

	cJSON * receivedJsonHandle = cJSON_CreateObject();
	cJSON_AddItemToObject(receivedJsonHandle, "timestamp", cJSON_CreateString(configPtr->received.timestampStr));
	cJSON_AddItemToObject(receivedJsonHandle, "exchangeId", cJSON_CreateString(configPtr->received.exchangeIdStr));
	cJSON_AddItemToObject(receivedJsonHandle, "tags", cJSON_Duplicate(configPtr->received.tagsJsonHandle,true));
	cJSON_AddNumberToObject(receivedJsonHandle, "delay", configPtr->received.delay2ApplyConfigSeconds);
	cJSON_AddItemToObject(receivedJsonHandle, "apply", appRuntimeConfig_GetApplyFlagAsJsonString(configPtr->received.applyFlag));

	cJSON_AddItemToObject(receivedJsonHandle, "baseTopic", cJSON_CreateString(configPtr->received.baseTopic));
	cJSON_AddItemToObject(receivedJsonHandle, "methodCreate", cJSON_CreateString(configPtr->received.methodCreate));
	cJSON_AddItemToObject(receivedJsonHandle, "methodUpdate", cJSON_CreateString(configPtr->received.methodUpdate));

	cJSON_AddItemToObject(jsonHandle, "received", receivedJsonHandle);

	return jsonHandle;
}
/**
 * @brief Returns the entire runtime config as JSON for persisting to file.
 * @return cJSON *: the JSON
 */
static cJSON * appRuntimeConfig_GetAsJsonObject2Persist(void) {

	cJSON * jsonHandle = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonHandle, "topicConfig", appRuntimeConfig_GetTopicConfigAsJsonObject(appRuntimeConfigPtr->topicConfigPtr));
	cJSON_AddItemToObject(jsonHandle, "mqttBrokerConnectionConfig", appRuntimeConfig_GetMqttBrokerConnectionConfigAsJsonObject(appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr));
	cJSON_AddItemToObject(jsonHandle, "statusConfig", appRuntimeConfig_GetStatusConfigAsJsonObject(appRuntimeConfigPtr->statusConfigPtr));
	cJSON_AddItemToObject(jsonHandle, "targetTelemetryConfig", appRuntimeConfig_GetTargetTelemetryConfigAsJsonObject(appRuntimeConfigPtr->targetTelemetryConfigPtr));

	return jsonHandle;

}

/**
 * @brief Returns the configuration for configElement type as a json object. Thread-safe.
 *
 * @warning Blocks access to the internal configuration pointer, don't call while it is blocked.
 *
 * @param[in] configElement : specifies which element (or all) of the runtime config to return.
 *
 * @return cJSON * : the created json object. Caller must delete it afterwards.
 *
 * @exception Retcode_RaiseError: from @ref appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr()
 *
 */
cJSON * AppRuntimeConfig_GetAsJsonObject(const AppRuntimeConfig_ConfigElement_T configElement) {

	cJSON * jsonHandle = NULL;

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	switch(configElement) {
	case AppRuntimeConfig_Element_topicConfig:
		jsonHandle = appRuntimeConfig_GetTopicConfigAsJsonObject(appRuntimeConfigPtr->topicConfigPtr);
		break;
	case AppRuntimeConfig_Element_mqttBrokerConnectionConfig:
		jsonHandle = appRuntimeConfig_GetMqttBrokerConnectionConfigAsJsonObject(appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr);
		break;
	case AppRuntimeConfig_Element_statusConfig:
		jsonHandle = appRuntimeConfig_GetStatusConfigAsJsonObject(appRuntimeConfigPtr->statusConfigPtr);
		break;
	case AppRuntimeConfig_Element_activeTelemetryRTParams:
		jsonHandle = appRuntimeConfig_GetActiveTelemetryRTParamsAsJsonObject(appRuntimeConfigPtr->activeTelemetryRTParamsPtr);
		break;
	case AppRuntimeConfig_Element_targetTelemetryConfig:
		jsonHandle = appRuntimeConfig_GetTargetTelemetryConfigAsJsonObject(appRuntimeConfigPtr->targetTelemetryConfigPtr);
		break;
	case AppRuntimeConfig_Element_AppRuntimeConfig: {
		jsonHandle = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonHandle, "topicConfig", appRuntimeConfig_GetTopicConfigAsJsonObject(appRuntimeConfigPtr->topicConfigPtr));
		cJSON_AddItemToObject(jsonHandle, "mqttBrokerConnectionConfig", appRuntimeConfig_GetMqttBrokerConnectionConfigAsJsonObject(appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr));
		cJSON_AddItemToObject(jsonHandle, "statusConfig", appRuntimeConfig_GetStatusConfigAsJsonObject(appRuntimeConfigPtr->statusConfigPtr));
		cJSON_AddItemToObject(jsonHandle, "activeTelemetryRTParams", appRuntimeConfig_GetActiveTelemetryRTParamsAsJsonObject(appRuntimeConfigPtr->activeTelemetryRTParamsPtr));
		cJSON_AddItemToObject(jsonHandle, "targetTelemetryConfig", appRuntimeConfig_GetTargetTelemetryConfigAsJsonObject(appRuntimeConfigPtr->targetTelemetryConfigPtr));
	}
		break;
	default: assert(0);
	}

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return jsonHandle;
}
/**
 * @brief Free the telemetry runtime parameters pointer.
 * @param[in] configPtr: the pointer to free
 */
static void appRuntimeConfig_DeleteTelemetryRTParams(AppRuntimeConfig_TelemetryRTParams_T * configPtr) {

	assert(configPtr);

	free(configPtr);
}
/**
 * @brief External interface to delete the telemetry runtime parameters pointer. Blocks / allows access to the runtime config.
 * @param[in] configPtr: the pointer
 */
void AppRuntimeConfig_DeleteTelemetryRTParams(AppRuntimeConfig_TelemetryRTParams_T * configPtr) {

	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	appRuntimeConfig_DeleteTelemetryRTParams(configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

}
/**
 * @brief Free the telemetry config.
 * @param[in] configPtr: the telemetry config
 */
static void appRuntimeConfig_DeleteTelemetryConfig(AppRuntimeConfig_TelemetryConfig_T * configPtr) {
	assert(configPtr);
	// delete all the pointers
	if(configPtr->received.timestampStr) free(configPtr->received.timestampStr);
	if(configPtr->received.exchangeIdStr) free(configPtr->received.exchangeIdStr);
	if(configPtr->received.tagsJsonHandle) cJSON_Delete(configPtr->received.tagsJsonHandle);
	free(configPtr);
}
/**
 * @brief Free the status config.
 * @param[in] configPtr: the status config
 */
static void appRuntimeConfig_DeleteStatusConfig(AppRuntimeConfig_StatusConfig_T * configPtr) {

	assert(configPtr);

	if(configPtr->received.timestampStr) free(configPtr->received.timestampStr);
	if(configPtr->received.exchangeIdStr) free(configPtr->received.exchangeIdStr);
	if(configPtr->received.tagsJsonHandle) cJSON_Delete(configPtr->received.tagsJsonHandle);

	free(configPtr);
}
/**
 * @brief Free the runtime config.
 * @param[in] configPtr: the runtime config
 */
static void appRuntimeConfig_Delete(AppRuntimeConfig_T * configPtr) {

	assert(configPtr);

	appRuntimeConfig_DeleteTopicConfig(configPtr->topicConfigPtr);
	appRuntimeConfig_DeleteMqttBrokerConnectionConfig(configPtr->mqttBrokerConnectionConfigPtr);
	appRuntimeConfig_DeleteTelemetryRTParams(configPtr->activeTelemetryRTParamsPtr);
	appRuntimeConfig_DeleteTelemetryConfig(configPtr->targetTelemetryConfigPtr);
	appRuntimeConfig_DeleteStatusConfig(configPtr->statusConfigPtr);

	free(configPtr);
}
/**
 * @brief Create a new topic config, either from the default config or the empty config.
 * @param[in] isFromDefault: flag if to be created from the default structure
 * @return AppRuntimeConfig_TopicConfig_T *: the new topic config
 */
static AppRuntimeConfig_TopicConfig_T * appRuntimeConfig_CreateTopicConfig(bool isFromDefault) {
	AppRuntimeConfig_TopicConfig_T * newPtr = NULL;
	if(isFromDefault) {
		newPtr = malloc(sizeof(appRuntimeConfig_DefaultTopicConfig));
		*newPtr = appRuntimeConfig_DefaultTopicConfig;
		newPtr->received.tagsJsonHandle = cJSON_Parse(APP_RT_CFG_DEFAULT_TAGS_JSON);
		newPtr->received.exchangeIdStr = copyString(APP_RT_CFG_DEFAULT_EXCHANGE_ID);
		newPtr->received.methodCreate = copyString(APP_RT_CFG_DEFAULT_TOPIC_METHOD_CREATE);
		newPtr->received.methodUpdate = copyString(APP_RT_CFG_DEFAULT_TOPIC_METHOD_UPDATE);
	}
	else {
		newPtr = malloc(sizeof(appRuntimeConfig_EmptyTopicConfig));
		*newPtr = appRuntimeConfig_EmptyTopicConfig;
	}
	return newPtr;
}
/**
 * @brief External interface for @ref appRuntimeConfig_CreateTopicConfig(). Creates the empty topic config.
 * @return AppRuntimeConfig_TopicConfig_T *: the empty topic config.
 */
AppRuntimeConfig_TopicConfig_T * AppRuntimeConfig_CreateTopicConfig(void) {
	return appRuntimeConfig_CreateTopicConfig(false);
}
/**
 * @brief Duplicates the topic config into a new config structure. Blocks/allows access to the runtime config.
 * @param[in] orgConfigPtr: the original topic config
 * @return AppRuntimeConfig_TopicConfig_T *: the duplicate topic config
 */
AppRuntimeConfig_TopicConfig_T * AppRuntimeConfig_DuplicateTopicConfig(const AppRuntimeConfig_TopicConfig_T * orgConfigPtr) {

	assert(orgConfigPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfig_TopicConfig_T * newConfigPtr = malloc(sizeof(AppRuntimeConfig_TopicConfig_T));
	*newConfigPtr = *orgConfigPtr;
	// copy the pointers' data
	newConfigPtr->received.timestampStr = copyString(orgConfigPtr->received.timestampStr);
	newConfigPtr->received.exchangeIdStr = copyString(orgConfigPtr->received.exchangeIdStr);
	newConfigPtr->received.tagsJsonHandle = cJSON_Duplicate(orgConfigPtr->received.tagsJsonHandle, true);

	newConfigPtr->received.baseTopic = copyString(orgConfigPtr->received.baseTopic);
	newConfigPtr->received.methodCreate = copyString(orgConfigPtr->received.methodCreate);
	newConfigPtr->received.methodUpdate = copyString(orgConfigPtr->received.methodUpdate);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return newConfigPtr;
}
/**
 * @brief Free the topic config.
 * @param[in] configPtr: the topic config
 */
static void appRuntimeConfig_DeleteTopicConfig(AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(configPtr);

	if(configPtr->received.timestampStr) free(configPtr->received.timestampStr);
	if(configPtr->received.exchangeIdStr) free(configPtr->received.exchangeIdStr);
	if(configPtr->received.tagsJsonHandle) cJSON_Delete(configPtr->received.tagsJsonHandle);

	if(configPtr->received.baseTopic) free(configPtr->received.baseTopic);
	if(configPtr->received.methodCreate) free(configPtr->received.methodCreate);
	if(configPtr->received.methodUpdate) free(configPtr->received.methodUpdate);

	free(configPtr);
}
/**
 * @brief External interface to free the topic config. Blocks/allows access the runtime config.
 * @param[in] configPtr: the topic config.
 */
void AppRuntimeConfig_DeleteTopicConfig(AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	appRuntimeConfig_DeleteTopicConfig(configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();
}
/**
 * @brief Create a new broker config, either from the default or the empty config.
 * @param[in] isFromDefault: flag if to be created from the default structure
 * @return AppRuntimeConfig_MqttBrokerConnectionConfig_T *: the new broker config
 */
static AppRuntimeConfig_MqttBrokerConnectionConfig_T * appRuntimeConfig_CreateMqttBrokerConnectionConfig(bool isFromDefault) {
	AppRuntimeConfig_MqttBrokerConnectionConfig_T * newPtr = NULL;
	if(isFromDefault) {
		newPtr = malloc(sizeof(appRuntimeConfig_DefaultMqttBrokerConnectionConfig));
		*newPtr = appRuntimeConfig_DefaultMqttBrokerConnectionConfig;
		newPtr->received.tagsJsonHandle = cJSON_Parse(APP_RT_CFG_DEFAULT_TAGS_JSON);
		newPtr->received.exchangeIdStr = copyString(APP_RT_CFG_DEFAULT_EXCHANGE_ID);
	}
	else {
		newPtr = malloc(sizeof(appRuntimeConfig_EmptyMqttBrokerConnectionConfig));
		*newPtr = appRuntimeConfig_EmptyMqttBrokerConnectionConfig;
	}
	return newPtr;
}
/**
 * @brief External interface for @ref appRuntimeConfig_CreateMqttBrokerConnectionConfig(). Creates the empty broker config.
 * @return AppRuntimeConfig_MqttBrokerConnectionConfig_T *: the empty broker config.
 */
AppRuntimeConfig_MqttBrokerConnectionConfig_T * AppRuntimeConfig_CreateMqttBrokerConnectionConfig(void) {
	return appRuntimeConfig_CreateMqttBrokerConnectionConfig(false);
}
/**
 * @brief Free the broker config.
 * @param[in] configPtr: the broker config
 */
static void appRuntimeConfig_DeleteMqttBrokerConnectionConfig(AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr) {
	assert(configPtr);

	if(configPtr->received.timestampStr) free(configPtr->received.timestampStr);
	if(configPtr->received.exchangeIdStr) free(configPtr->received.exchangeIdStr);
	if(configPtr->received.tagsJsonHandle) cJSON_Delete(configPtr->received.tagsJsonHandle);

	if(configPtr->received.brokerUrl) free(configPtr->received.brokerUrl);
	if(configPtr->received.brokerUsername) free(configPtr->received.brokerUsername);
	if(configPtr->received.brokerPassword) free(configPtr->received.brokerPassword);

	free(configPtr);
}
/**
 * @brief External interface to free the sensors config. Blocks/allows access to the runtime config.
 * @param[in] sensorsPtr: the sensors config
 */
void AppRuntimeConfig_DeleteSensors(AppRuntimeConfig_Sensors_T * sensorsPtr) {

	assert(sensorsPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	free(sensorsPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

}
/**
 * @brief External interface to duplicate the sensors config. Blocks/allows access to the runtime config.
 * @param[in] orgSensorsPtr: the sensors config to duplicate
 * @return AppRuntimeConfig_Sensors_T *: the new sensors config
 */
AppRuntimeConfig_Sensors_T * AppRuntimeConfig_DuplicateSensors(const AppRuntimeConfig_Sensors_T * orgSensorsPtr) {

	assert(orgSensorsPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfig_Sensors_T * newSensorsPtr = malloc(sizeof(AppRuntimeConfig_Sensors_T));
	*newSensorsPtr = *orgSensorsPtr;

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return newSensorsPtr;
}
/**
 * @brief Create a new status config either from the default or empty structure.
 * @param[in] isFromDefault: flag to indicate if it is to be created from the default structure
 * @return AppRuntimeConfig_StatusConfig_T *: the new status config
 */
static AppRuntimeConfig_StatusConfig_T * appRuntimeConfig_CreateStatusConfig(bool isFromDefault) {
	AppRuntimeConfig_StatusConfig_T * newPtr = NULL;
	if(isFromDefault) {
		newPtr = malloc(sizeof(appRuntimeConfig_DefaultStatusConfig));
		*newPtr = appRuntimeConfig_DefaultStatusConfig;
		newPtr->received.tagsJsonHandle = cJSON_Parse(APP_RT_CFG_DEFAULT_TAGS_JSON);
		newPtr->received.exchangeIdStr = copyString(APP_RT_CFG_DEFAULT_EXCHANGE_ID);
	}
	else {
		newPtr = malloc(sizeof(appRuntimeConfig_EmptyStatusConfig));
		*newPtr = appRuntimeConfig_EmptyStatusConfig;
	}
	return newPtr;
}
/**
 * @brief External interface for @ref appRuntimeConfig_CreateStatusConfig(). Creates from default.
 * @return AppRuntimeConfig_StatusConfig_T *: the new status config
 */
AppRuntimeConfig_StatusConfig_T * AppRuntimeConfig_CreateStatusConfig(void) {
	return appRuntimeConfig_CreateStatusConfig(false);
}
/**
 * @brief Create a new telemetry runtime parameters config from the empty structure.
 * @note This config has no default values, they are set at inititalization from target telemetry config.
 * @return AppRuntimeConfig_TelemetryRTParams_T *: the created config
 */
static AppRuntimeConfig_TelemetryRTParams_T * appRuntimeConfig_CreateTelemetryRTParams(void) {

	AppRuntimeConfig_TelemetryRTParams_T * newPtr = malloc(sizeof(appRuntimeConfig_EmptyActiveTelemetryRTParams));
	*newPtr = appRuntimeConfig_EmptyActiveTelemetryRTParams;
	return newPtr;
}
/**
 * @brief External interface for @ref appRuntimeConfig_CreateTelemetryRTParams().
 * @return AppRuntimeConfig_TelemetryRTParams_T *: the created config
 */
AppRuntimeConfig_TelemetryRTParams_T * AppRuntimeConfig_CreateTelemetryRTParams(void) {
	return appRuntimeConfig_CreateTelemetryRTParams();
}
/**
 * @brief Duplicate the given telemetry runtime parameters config.
 * @param[in] orgConfigPtr: the config to be duplicated
 * @return AppRuntimeConfig_TelemetryRTParams_T *: the duplicated config
 */
static AppRuntimeConfig_TelemetryRTParams_T * appRuntimeConfig_DuplicateTelemetryRTParams(const AppRuntimeConfig_TelemetryRTParams_T * orgConfigPtr) {

	AppRuntimeConfig_TelemetryRTParams_T * newConfigPtr = malloc(sizeof(*orgConfigPtr));
	*newConfigPtr = *orgConfigPtr;
	return newConfigPtr;
}
/**
 * @brief Create a new telemetry configuration either from the default or empty structure.
 * @param[in] isFromDefault: flag, create from default if true, from empty if false
 * @return AppRuntimeConfig_TelemetryConfig_T *: the new config
 */
static AppRuntimeConfig_TelemetryConfig_T * appRuntimeConfig_CreateTelemetryConfig(bool isFromDefault) {
	AppRuntimeConfig_TelemetryConfig_T * newPtr = NULL;
	if(isFromDefault) {
		newPtr = malloc(sizeof(appRuntimeConfig_DefaultTargetTelemetryConfig));
		*newPtr = appRuntimeConfig_DefaultTargetTelemetryConfig;
		newPtr->received.tagsJsonHandle = cJSON_Parse(APP_RT_CFG_DEFAULT_TAGS_JSON);
		newPtr->received.exchangeIdStr = copyString(APP_RT_CFG_DEFAULT_EXCHANGE_ID);
	}
	else {
		newPtr = malloc(sizeof(appRuntimeConfig_EmptyTargetTelemetryConfig));
		*newPtr = appRuntimeConfig_EmptyTargetTelemetryConfig;
	}
	return newPtr;
}
/**
 * @brief External interface for @ref appRuntimeConfig_CreateTelemetryConfig(), creating an empty config.
 * @return AppRuntimeConfig_TelemetryConfig_T *: the new config
 */
AppRuntimeConfig_TelemetryConfig_T * AppRuntimeConfig_CreateTelemetryConfig(void) {
	return appRuntimeConfig_CreateTelemetryConfig(false);
}
/**
 * @brief Create a complete runtime config either from default or the empty structure.
 * @param[in] isFromDefault: flag, create from default if true, from empty if false
 * @return AppRuntimeConfig_T *: the new config
 */
static AppRuntimeConfig_T * appRuntimeConfig_CreateAppRuntimeConfig(bool isFromDefault) {
	AppRuntimeConfig_T * newPtr = NULL;

	if(isFromDefault) {
		newPtr = malloc(sizeof(appRuntimeConfig_DefaultAppRuntimeConfig));
		*newPtr = appRuntimeConfig_DefaultAppRuntimeConfig;
	}
	else {
		newPtr = malloc(sizeof(appRuntimeConfig_EmptyAppRuntimeConfig));
		*newPtr = appRuntimeConfig_EmptyAppRuntimeConfig;
	}

	newPtr->targetTelemetryConfigPtr = appRuntimeConfig_CreateTelemetryConfig(isFromDefault);
	newPtr->activeTelemetryRTParamsPtr = appRuntimeConfig_CreateTelemetryRTParams();
	newPtr->statusConfigPtr = appRuntimeConfig_CreateStatusConfig(isFromDefault);
	newPtr->mqttBrokerConnectionConfigPtr = appRuntimeConfig_CreateMqttBrokerConnectionConfig(isFromDefault);
	newPtr->topicConfigPtr = appRuntimeConfig_CreateTopicConfig(isFromDefault);

	return newPtr;
}
/**
 * @brief Serialize the runtime config to a given buffer.
 * @param[in,out] buffer: pointer to the pre-allocated buffer. Contains the serialized, NULL terminated config afterwards
 * @param[out] bufferLength: pointer to the length of the buffer. Set here to actual length including terminating NULL
 */
static void appRuntimeConfig_SerializeAppRuntimeConfig2Buffer(uint8_t * buffer, size_t * bufferLength) {

	cJSON * bufferJsonHandle = appRuntimeConfig_GetAsJsonObject2Persist();

	char * jsonStr = cJSON_PrintUnformatted(bufferJsonHandle);
	*bufferLength = strlen(jsonStr)+1;

	memcpy(buffer, jsonStr, *bufferLength);
	free(jsonStr);
	cJSON_Delete(bufferJsonHandle);
}
/**
 * @brief Delete the runtime config file from the SD card. Does nothing if SD card not available.
 */
static void appRuntimeConfig_DeleteRuntimeConfigFile(void) {
	Retcode_T retcode = RETCODE_OK;

	bool status = false;
	if (RETCODE_OK == retcode) retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &status);

	if ((RETCODE_OK != retcode) && (status == false)) return;

	retcode = Storage_Delete(STORAGE_MEDIUM_SD_CARD, RT_CFG_FILE_NAME);
	if(RETCODE_OK != retcode) {
		printf("[INFO] - appRuntimeConfig_DeleteRuntimeConfigFile: can't delete file, probably doesn't exist. \r\n");
	} else {
		printf("[INFO] - appRuntimeConfig_DeleteRuntimeConfigFile: deleted. \r\n");
	}
}
/**
 * @brief Persist active runtime config to SD card.
 * First, it writes a new file then it renames it to the correct file name.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_SD_CARD_NOT_AVAILABLE)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_SD_CARD_FAILED_TO_WRITE_RT_CONFIG)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_SD_CARD_FAILED_TO_RENAME_RT_CONFIG_FILE)
 *
 */
static Retcode_T appRuntimeConfig_PersistRuntimeConfig(void) {

	Retcode_T retcode = RETCODE_OK;

	bool status = false;
	if (RETCODE_OK == retcode) retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &status);

	if ((RETCODE_OK != retcode) && (status == false)) {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_SD_CARD_NOT_AVAILABLE);
	}

	// serialize the runtime config
	size_t bytes2Write = 0;
	appRuntimeConfig_SerializeAppRuntimeConfig2Buffer(appRuntimeConfig_FileWriteBuffer, &bytes2Write);
	appRuntimeConfig_StorageWrite.BytesToWrite = bytes2Write;

#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - appRuntimeConfig_PersistRuntimeConfig: appRuntimeConfig_FileWriteBuffer=\r\n");
	printf("number of bytes: %i\r\n", bytes2Write);
	printf("%s\r\n", appRuntimeConfig_FileWriteBuffer);
#endif

	// if the new runtime config file exists it will be deleted and then written
	retcode = Storage_Write(STORAGE_MEDIUM_SD_CARD, &appRuntimeConfig_StorageWrite);
	if (retcode!= RETCODE_OK) {
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_SD_CARD_FAILED_TO_WRITE_RT_CONFIG);
	}
	// rename new file: deletes old file first if exists
	retcode = Storage_Rename(STORAGE_MEDIUM_SD_CARD, &appRuntimeConfig_StorageRenameInfo);
	if (retcode!= RETCODE_OK) {
		printf("[ERROR] - AppRuntimeConfig.persistRuntimeConfig: Cannot rename new runtime configuration file.\r\n");
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_SD_CARD_FAILED_TO_RENAME_RT_CONFIG_FILE);
	}
	return retcode;
}
/**
 * @brief Reads the runtime config from file on SD card.
 * @return cJSON *: the parsed JSON or NULL if it doesn't exist or cannot be parsed
 * @exception AppStatus_SendStatusMessage: @ref AppStatus_SendStatusMessage(#AppStatus_CreateMessage(#AppStatusMessage_Status_Error, #AppStatusMessage_Descr_ErrorParsingRuntimeConfigFileBefore, cJSON_GetErrorPtr()))
 */
static cJSON * appRuntimeConfig_ReadRuntimeConfigFromFile(void) {

	Retcode_T retcode = RETCODE_OK;

	bool status = false;
	if (RETCODE_OK == retcode) retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &status);

	if ((RETCODE_OK == retcode) && (status == true)) {

		uint8_t FileReadBuffer[RT_CFG_FILE_BUFFER_SIZE] = { 0 };

		Storage_Read_T storageReadCredentials = {
			.FileName = RT_CFG_FILE_NAME,
			.ReadBuffer = FileReadBuffer,
			.BytesToRead = sizeof(FileReadBuffer),
			.ActualBytesRead = 0UL,
			.Offset = 0UL
		};

		retcode = Storage_Read(STORAGE_MEDIUM_SD_CARD, &storageReadCredentials);

		if(RETCODE_OK != retcode) {
			// config file may not exist.
			#ifdef DEBUG_APP_RUNTIME_CONFIG
			printf("[INFO] - appRuntimeConfig_ReadRuntimeConfigFromFile: Cannot read runtime configuration file on SD Card.\r\n");
			#endif
			return NULL;
		}
		cJSON * configJSON = cJSON_Parse((const char *) FileReadBuffer);
		if (!configJSON) {
			#ifdef DEBUG_APP_RUNTIME_CONFIG
			printf("[ERROR] - appRuntimeConfig_ReadRuntimeConfigFromFile: parsing config file, before: [%s]\r\n", cJSON_GetErrorPtr());
			#endif
			AppStatus_SendStatusMessage(AppStatus_CreateMessage(AppStatusMessage_Status_Error, AppStatusMessage_Descr_ErrorParsingRuntimeConfigFileBefore, cJSON_GetErrorPtr()));
			return NULL;
		}

		#ifdef DEBUG_APP_RUNTIME_CONFIG
		printf("[INFO] - appRuntimeConfig_ReadRuntimeConfigFromFile: parsed OK. %s:\r\n", RT_CFG_FILE_NAME);
		printJSON(configJSON);
		#endif
		return configJSON;
	}
	return NULL;
}
/**
 * @brief Validate telemetry runtime parameters.
 * @param[in] rtParamsPtr: the telemetry runtime parameters config
 * @param[in,out] statusPtr: the status with success=false or true. if false, details are set.
 */
static void appRuntimeConfig_ValidateTelemetryRTParams(AppRuntimeConfig_TelemetryRTParams_T const * const rtParamsPtr, AppRuntimeConfigStatus_T * statusPtr) {

	assert(rtParamsPtr);
	assert(statusPtr);

	statusPtr->success = true;

	if( rtParamsPtr->numberOfSamplesPerEvent == 0 ) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_InvalidTimingsOrFrequency;
		statusPtr->details = copyString("numberOfSamplesPerEvent is 0");
		return;
	}
	if( rtParamsPtr->publishPeriodcityMillis == 0 ) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_InvalidTimingsOrFrequency;
		statusPtr->details = copyString("publishPeriodcityMillis is 0");
		return;
	}
	if( rtParamsPtr->samplingPeriodicityMillis == 0 ) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_InvalidTimingsOrFrequency;
		statusPtr->details = copyString("samplingPeriodicityMillis is 0");
		return;
	}
	if( rtParamsPtr->publishPeriodcityMillis < rtParamsPtr->samplingPeriodicityMillis) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_InvalidTimingsOrFrequency;
		statusPtr->details = copyString("publishPeriodcityMillis < samplingPeriodicityMillis");
		return;
	}

}
/**
 * @brief Create a temporary test queue and populates it with telemetry samples.
 * Used to validate that a configuration for number of samples per queue in combination with
 * the payload format does not exceed the max data length (#APP_MQTT_MAX_PUBLISH_DATA_LENGTH) for a single MQTT message.
 *
 * @param[in] numSamplesPerEvent: the number of telemetry samples in one message
 * @param[in] sensorsConfigPtr: the sensor configuration, determines which sensor readings to add to the samples
 * @param[in] payloadFormat: which payload format is to be applied
 * @param[in,out] statusPtr: the status pointer, contains the result status of the validation
 *
 */
static void appRuntimeConfig_ValidateTelemetryQueueSize(
		uint8_t numSamplesPerEvent,
		AppRuntimeConfig_Sensors_T * sensorsConfigPtr,
		AppRuntimeConfig_Telemetry_PayloadFormat_T payloadFormat,
		AppRuntimeConfigStatus_T * statusPtr) {

	statusPtr->success = true;
	Retcode_T retcode = RETCODE_OK;

	Sensor_Value_T sensorValue;
	memset(&sensorValue, 0x00, sizeof(sensorValue));
	retcode = Sensor_GetData(&sensorValue);
	if(RETCODE_OK != retcode) assert(0);

	AppTelemetryQueueCreateNewTestQueue(numSamplesPerEvent);

	for(int i = 0; i < numSamplesPerEvent; i++) {

		AppTelemetryPayload_T * payloadPtr = AppTelemetryPayload_CreateNew_Test(xTaskGetTickCount(), &sensorValue, sensorsConfigPtr, payloadFormat);

		AppTelemetryQueueTestQueueAddSample(payloadPtr);
	}

	uint32_t queueDataLength = AppTelemetryQueueTestQueueGetDataSize();

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - appRuntimeConfig_ValidateTelemetryQueueSize: \r\n");
	printf(" - queueDataLength:%lu \r\n", queueDataLength);
	printf(" - queue contents:\r\n");
	AppTelemetryQueueTestQueuePrint();
	#endif

	AppTelemetryQueueTestQueueDelete();

	if(queueDataLength > APP_MQTT_MAX_PUBLISH_DATA_LENGTH) {
		#ifdef DEBUG_APP_RUNTIME_CONFIG
		printf("[ERROR] - appRuntimeConfig_ValidateTelemetryQueueSize: queueDataLength:%lu > APP_MQTT_MAX_PUBLISH_DATA_LENGTH:%lu\r\n", queueDataLength, APP_MQTT_MAX_PUBLISH_DATA_LENGTH);
		#endif
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_TelemetryMessageTooLarge;
		statusPtr->details = copyString("sensor / samplesPerEvent combination yields too large a message for the XDK");
		return;
	}
}
/**
 * @brief Read the 'timestamp' JSON element from jsonHandle.
 *
 * @param[in] jsonHandle: the JSON which contains an element 'timestamp'
 * @param[in,out] timestampStrPtr: the copy of the timestamp string from the JSON
 * @param[in,out] statusPtr: the return status, set to false if 'timestamp' element not found
 *
 * @return bool: success or failed
 *
 * **Example Usage:**
 * @code
 * 	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();
 * 	statusPtr->success = true;
 *
 *  char * timestampStr = NULL;
 *  if(!appRuntimeConfig_ReadConfigTimestampStr(jsonHandle, &timestampStr, statusPtr)) return statusPtr;
 *
 * @endcode
 *
 */
static bool appRuntimeConfig_ReadConfigTimestampStr(const cJSON * jsonHandle, char ** timestampStrPtr, AppRuntimeConfigStatus_T * statusPtr) {
	// 'timestamp' element - mandatory
	cJSON * t_jsonHandle = (cJSON*)jsonHandle; // get rid of const warning
	cJSON * timestampJsonHandle = cJSON_GetObjectItem(t_jsonHandle, "timestamp");
	if(timestampJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement;
		statusPtr->details = copyString("timestamp");
		return false;
	}
	*timestampStrPtr = copyString(timestampJsonHandle->valuestring);
	return true;
}
/**
 * @brief Read the 'exchangeId' element from the JSON.
 * @param[in] jsonHandle: the JSON
 * @param[in,out] exchangeIdStrPtr: the copy of the exchangeId string in the JSON
 * @param[in,out] statusPtr: the return status, set to false if 'exchangeId' element not found
 *
 * @return bool: success or failed
 */
static bool appRuntimeConfig_ReadConfigExchangeIdJson(const cJSON * jsonHandle, char ** exchangeIdStrPtr, AppRuntimeConfigStatus_T * statusPtr) {
	// 'exchangeId' element - mandatory
	cJSON * exchangeIdJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "exchangeId");
	if(exchangeIdJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement;
		statusPtr->details = copyString("exchangeId");
		return false;
	}
	*exchangeIdStrPtr = copyString(exchangeIdJsonHandle->valuestring);
	return true;
}
/**
 * @brief Read the 'tags' object from the JSON.
 * @param[in] jsonHandle: the JSON
 * @param[in,out] tagsJsonPtrPtr: the copy/duplicate of the tags JSON object found in jsonHandle
 * @param[in,out] statusPtr: the return status, set to false if 'tags' element not found
 *
 * @return bool: success or failed
 */
static bool appRuntimeConfig_ReadConfigTagsJson(const cJSON * jsonHandle, cJSON ** tagsJsonPtrPtr, AppRuntimeConfigStatus_T * statusPtr) {
	// 'tags' element - mandatory
	cJSON * tagsJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "tags");
	if(tagsJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement;
		statusPtr->details = copyString("tags");
		return false;
	}
	*tagsJsonPtrPtr = cJSON_Duplicate(tagsJsonHandle, true);
	return true;
}
/**
 * @brief Read the optional 'delay' element in the JSON.
 *
 * @param[in] jsonHandle: the JSON
 * @param[in,out] delaySecondsPtr: the value of the 'delay' element. If not found, set to 1 by default.
 * @param[in,out] statusPtr: the return status, set to false if 'delay' negative or greater than 10
 *
 * @return bool: success or failed
 */
static bool appRuntimeConfig_ReadConfigDelay(const cJSON * jsonHandle, uint8_t * delaySecondsPtr, AppRuntimeConfigStatus_T * statusPtr) {
	// 'delay' element - optional
	*delaySecondsPtr = 1;
	cJSON * delayJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "delay");
	if(delayJsonHandle == NULL) {
		return true;
	} else {
		*delaySecondsPtr = delayJsonHandle->valueint;
		if(delayJsonHandle->valueint < 0) {
			statusPtr->success = false;
			statusPtr->descrCode = AppStatusMessage_Descr_DelayIsNegative;
			statusPtr->details = copyString("must be: 0 < delay < 10");
			return false;
		} else if(*delaySecondsPtr > 10 ) {
			statusPtr->success = false;
			statusPtr->descrCode = AppStatusMessage_Descr_DelayGt10;
			statusPtr->details = copyString("must be: 0 < delay < 10");
			return false;
		}
	}
	return true;
}
/**
 * @brief Read the optional 'apply' element in the JSON.
 *
 * @param[in] jsonHandle: the JSON
 * @param[in,out] applyFlagPtr: the value of the 'apply' element. If not found, set to #APP_RT_CFG_DEFAULT_APPLY
 * @param[in,out] statusPtr: the return status, set to false if 'apply' value not valid
 *
 * @return bool: success or failed
 */
static bool appRuntimeConfig_ReadConfigApply(const cJSON * jsonHandle, AppRuntimeConfig_Apply_T * applyFlagPtr, AppRuntimeConfigStatus_T * statusPtr) {

	// 'apply' element - optional

	*applyFlagPtr = APP_RT_CFG_DEFAULT_APPLY;
	cJSON * applyJsonHandle = cJSON_GetObjectItem((cJSON *)jsonHandle, "apply");
	if(applyJsonHandle == NULL) {
		return true;
	} else {
		if(NULL != strstr(applyJsonHandle->valuestring, APP_RT_CFG_APPLY_TRANSIENT) ) *applyFlagPtr = AppRuntimeConfig_Apply_Transient;
		else if(NULL != strstr(applyJsonHandle->valuestring, APP_RT_CFG_APPLY_PERSISTENT) ) *applyFlagPtr = AppRuntimeConfig_Apply_Persistent;
		else {
			statusPtr->success = false;
			statusPtr->descrCode = AppStatusMessage_Descr_UnknownValue_Apply;
			statusPtr->details = copyString(applyJsonHandle->valuestring);
			return false;
		}
	}
	return true;
}
/**
 * @brief Validates the topic configuration contained in jsonHandle and returns the new topic configuration.
 * @param[in] jsonHandle: the JSON containing the new topic configuration
 * @param[in,out] configPtr: the populated topic configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();
	statusPtr->success = true;

	char * timestampStr = NULL;
	if(!appRuntimeConfig_ReadConfigTimestampStr(jsonHandle, &timestampStr, statusPtr)) return statusPtr;

    uint8_t delaySeconds;
	if(!appRuntimeConfig_ReadConfigDelay(jsonHandle, &delaySeconds, statusPtr)) return statusPtr;

	AppRuntimeConfig_Apply_T applyFlag;
	if(!appRuntimeConfig_ReadConfigApply(jsonHandle, &applyFlag, statusPtr)) return statusPtr;

	cJSON * tagsJsonHandle = NULL;
	if(!appRuntimeConfig_ReadConfigTagsJson(jsonHandle, &tagsJsonHandle, statusPtr)) return statusPtr;

	char * exchangeIdStr = NULL;
	if(!appRuntimeConfig_ReadConfigExchangeIdJson(jsonHandle, &exchangeIdStr, statusPtr)) return statusPtr;

	cJSON * baseTopic_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "baseTopic");
	if(baseTopic_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TopicConfig;
		statusPtr->details = copyString("baseTopic");
		return statusPtr;
	}
	if(countCharOccurencesInString(baseTopic_JsonHandle->valuestring, '/') != 2) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_TopicConfig_BaseTopicMustHave3Levels;
		statusPtr->details = copyString(baseTopic_JsonHandle->valuestring);
		return statusPtr;
	}
	cJSON * methodCreate_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "methodCreate");
	if(methodCreate_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TopicConfig;
		statusPtr->details = copyString("methodCreate");
		return statusPtr;
	}
	cJSON * methodUpdate_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "methodUpdate");
	if(methodUpdate_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TopicConfig;
		statusPtr->details = copyString("methodUpdate");
		return statusPtr;
	}

	// populate
	configPtr->received.timestampStr = timestampStr;
	configPtr->received.delay2ApplyConfigSeconds = delaySeconds;
	configPtr->received.applyFlag = applyFlag;
	configPtr->received.tagsJsonHandle = cJSON_Duplicate(tagsJsonHandle, true);
	configPtr->received.exchangeIdStr = exchangeIdStr;

	configPtr->received.baseTopic = copyString(baseTopic_JsonHandle->valuestring);
	configPtr->received.methodCreate = copyString(methodCreate_JsonHandle->valuestring);
	configPtr->received.methodUpdate = copyString(methodUpdate_JsonHandle->valuestring);

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - AppRuntimeConfig.populateAndValidateTopicConfigFromJSON: new & validated runtime config: \r\n");
	printf("[INFO] - topicConfig: \r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_topicConfig, configPtr);
	#endif

	return statusPtr;
}
/**
 * @brief Validates the mqtt broker configuration contained in jsonHandle and returns the new configuration.
 * @param[in] jsonHandle: the JSON containing the new mqtt broker configuration
 * @param[in,out] configPtr: the populated mqtt broker configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();
	statusPtr->success = true;

	char * timestampStr = NULL;
	if(!appRuntimeConfig_ReadConfigTimestampStr(jsonHandle, &timestampStr, statusPtr)) return statusPtr;

    uint8_t delaySeconds;
	if(!appRuntimeConfig_ReadConfigDelay(jsonHandle, &delaySeconds, statusPtr)) return statusPtr;

	AppRuntimeConfig_Apply_T applyFlag;
	if(!appRuntimeConfig_ReadConfigApply(jsonHandle, &applyFlag, statusPtr)) return statusPtr;

	cJSON * tagsJsonHandle = NULL;
	if(!appRuntimeConfig_ReadConfigTagsJson(jsonHandle, &tagsJsonHandle, statusPtr)) return statusPtr;

	char * exchangeIdStr = NULL;
	if(!appRuntimeConfig_ReadConfigExchangeIdJson(jsonHandle, &exchangeIdStr, statusPtr)) return statusPtr;

	cJSON * brokerURL_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "brokerURL");
	if(brokerURL_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("brokerURL");
		return statusPtr;
	}

	cJSON * brokerPort_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "brokerPort");
	if(brokerPort_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("brokerPort");
		return statusPtr;
	}

	cJSON * brokerUsername_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "brokerUsername");
	if(brokerUsername_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("brokerUsername");
		return statusPtr;
	}

	cJSON * brokerPassword_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "brokerPassword");
	if(brokerPassword_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("brokerPassword");
		return statusPtr;
	}

	cJSON * isCleanSession_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "cleanSession");
	if(isCleanSession_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("cleanSession");
		return statusPtr;
	}

	cJSON * isSecureConnection_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "secureConnection");
	if(isSecureConnection_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("secureConnection");
		return statusPtr;
	}

	cJSON * keepAliveIntervalSecs_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "keepAliveIntervalSecs");
	if(keepAliveIntervalSecs_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig;
		statusPtr->details = copyString("keepAliveIntervalSecs");
		return statusPtr;
	}

	// populate
	configPtr->received.timestampStr = timestampStr;
	configPtr->received.delay2ApplyConfigSeconds = delaySeconds;
	configPtr->received.applyFlag = applyFlag;
	configPtr->received.tagsJsonHandle = cJSON_Duplicate(tagsJsonHandle, true);
	configPtr->received.exchangeIdStr = exchangeIdStr;

	configPtr->received.brokerUrl= copyString(brokerURL_JsonHandle->valuestring);
	configPtr->received.brokerPort = brokerPort_JsonHandle->valueint;
	configPtr->received.brokerUsername = copyString(brokerUsername_JsonHandle->valuestring);
	configPtr->received.brokerPassword = copyString(brokerPassword_JsonHandle->valuestring);
	configPtr->received.isCleanSession = isCleanSession_JsonHandle->valueint;
	configPtr->received.isSecureConnection = isSecureConnection_JsonHandle->valueint;
	configPtr->received.keepAliveIntervalSecs = keepAliveIntervalSecs_JsonHandle->valueint;

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - appRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON: new & validated runtime config: \r\n");
	printf("[INFO] - mqttBrokerConnectionConfig: \r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_mqttBrokerConnectionConfig, configPtr);
	#endif

	return statusPtr;
}
/**
 * @brief Validates the status configuration contained in jsonHandle and returns the new configuration.
 * @param[in] jsonHandle: the JSON containing the new status configuration
 * @param[in,out] configPtr: the populated status configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_StatusConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();
	statusPtr->success = true;

	char * timestampStr = NULL;
	if(!appRuntimeConfig_ReadConfigTimestampStr(jsonHandle, &timestampStr, statusPtr)) return statusPtr;

    uint8_t delaySeconds;
	if(!appRuntimeConfig_ReadConfigDelay(jsonHandle, &delaySeconds, statusPtr)) return statusPtr;

	AppRuntimeConfig_Apply_T applyFlag;
	if(!appRuntimeConfig_ReadConfigApply(jsonHandle, &applyFlag, statusPtr)) return statusPtr;

	cJSON * tagsJsonHandle = NULL;
	if(!appRuntimeConfig_ReadConfigTagsJson(jsonHandle, &tagsJsonHandle, statusPtr)) return statusPtr;

	char * exchangeIdStr = NULL;
	if(!appRuntimeConfig_ReadConfigExchangeIdJson(jsonHandle, &exchangeIdStr, statusPtr)) return statusPtr;

	// mandatory
	cJSON * sendPeriodicStatus_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "sendPeriodicStatus");
	if(sendPeriodicStatus_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_StatusConfig;
		statusPtr->details = copyString("sendPeriodicStatus");
		return statusPtr;
	}
	bool isSendPeriodicStatus = sendPeriodicStatus_JsonHandle->valueint;

	// mandatory
	cJSON * periodicStatusIntervalSecs_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "periodicStatusIntervalSecs");
	if(periodicStatusIntervalSecs_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_StatusConfig;
		statusPtr->details = copyString("periodicStatusIntervalSecs");
		return statusPtr;
	}
	uint32_t periodicStatusIntervalSecs = periodicStatusIntervalSecs_JsonHandle->valueint;
	// min value
	if(APP_RT_CFG_STATUS_MIN_INTERVAL_SECS > periodicStatusIntervalSecs) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_StatusConfig_IntervalTooSmall;
		statusPtr->details = copyString("periodicStatusIntervalSecs");
		return statusPtr;
	}
	// mandatory
	cJSON * periodicStatusType_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "periodicStatusType");
	if(periodicStatusType_JsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_StatusConfig;
		statusPtr->details = copyString("periodicStatusType");
		return statusPtr;
	}
	AppRuntimeConfig_PeriodicStatusType_T periodicStatusType = APP_RT_CFG_DEFAULT_STATUS_PERIODIC_TYPE;
	if(NULL != strstr(periodicStatusType_JsonHandle->valuestring, APP_RT_CFG_STATUS_PERIODIC_TYPE_FULL) ) periodicStatusType = AppRuntimeConfig_PeriodicStatusType_Full;
	else if(NULL != strstr(periodicStatusType_JsonHandle->valuestring, APP_RT_CFG_STATUS_PERIODIC_TYPE_SHORT) ) periodicStatusType = AppRuntimeConfig_PeriodicStatusType_Short;
	else {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_StatusConfig_UnknownValue_PeriodicStatusType;
		statusPtr->details = copyString(periodicStatusType_JsonHandle->valuestring);
		return statusPtr;
	}

	// qos - optional
	uint32_t qos = APP_RT_CFG_DEFAULT_STATUS_QOS;
	cJSON * qos_JsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "qos");
	if(qos_JsonHandle != NULL) qos = qos_JsonHandle->valueint;
	if(qos != 0 && qos != 1) {
		qos = APP_RT_CFG_DEFAULT_STATUS_QOS;
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_StatusConfig_UnknownValue_QoS;
		statusPtr->details = copyString("qos");
		return statusPtr;
	}

	// populate
	configPtr->received.timestampStr = timestampStr;
	configPtr->received.delay2ApplyConfigSeconds = delaySeconds;
	configPtr->received.applyFlag = applyFlag;
	configPtr->received.tagsJsonHandle = cJSON_Duplicate(tagsJsonHandle, true);
	configPtr->received.exchangeIdStr = exchangeIdStr;

	configPtr->received.isSendPeriodicStatus = isSendPeriodicStatus;
	configPtr->received.periodicStatusIntervalSecs = periodicStatusIntervalSecs;
	configPtr->received.periodicStatusType = periodicStatusType;
	configPtr->received.qos = qos;

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - appRuntimeConfig_PopulateAndValidateStatusConfigFromJSON: new & validated runtime config: \r\n");
	printf("[INFO] - statusConfig: \r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_statusConfig, configPtr);
	#endif

	return statusPtr;

}
/**
 * @brief Validates the telemetry configuration contained in jsonHandle and returns the new configuration as well as the runtime parameters.
 * @param[in] jsonHandle: the JSON containing the new telemetry configuration
 * @param[in,out] configPtr: the populated telemetry configuration
 * @param[in,out] rtParamsPtr: the new telemetry runtime parameters
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TelemetryConfig_T * configPtr, AppRuntimeConfig_TelemetryRTParams_T * rtParamsPtr) {

	assert(jsonHandle);
	assert(configPtr);

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();
	statusPtr->success = true;

	char * timestampStr = NULL;
	if(!appRuntimeConfig_ReadConfigTimestampStr(jsonHandle, &timestampStr, statusPtr)) return statusPtr;

    uint8_t delaySeconds;
	if(!appRuntimeConfig_ReadConfigDelay(jsonHandle, &delaySeconds, statusPtr)) return statusPtr;

	AppRuntimeConfig_Apply_T applyFlag;
	if(!appRuntimeConfig_ReadConfigApply(jsonHandle, &applyFlag, statusPtr)) return statusPtr;

	cJSON * tagsJsonHandle = NULL;
	if(!appRuntimeConfig_ReadConfigTagsJson(jsonHandle, &tagsJsonHandle, statusPtr)) return statusPtr;

	char * exchangeIdStr = NULL;
	if(!appRuntimeConfig_ReadConfigExchangeIdJson(jsonHandle, &exchangeIdStr, statusPtr)) return statusPtr;

	cJSON * activateAtBootTimeJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "activateAtBootTime");
	if (activateAtBootTimeJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TelemetryConfig;
		statusPtr->details = copyString("activateAtBootTime");
		return statusPtr;
	}
	bool activateAtBootTime = activateAtBootTimeJsonHandle->valueint;

	// 'sensorsEnable' element - optional
	AppRuntimeConfig_SensorEnable_T sensorsEnableFlag = APP_RT_CFG_DEFAULT_SENSOR_ENABLE;
	cJSON * sensorsEnableJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "sensorsEnable");
	if(sensorsEnableJsonHandle != NULL) {
		if(NULL != strstr(sensorsEnableJsonHandle->valuestring, APP_RT_CFG_SENSORS_ENABLE_ALL) ) sensorsEnableFlag = AppRuntimeConfig_SensorEnable_All;
		else if(NULL != strstr(sensorsEnableJsonHandle->valuestring, APP_RT_CFG_SENSORS_ENABLE_SELECTED) ) sensorsEnableFlag = AppRuntimeConfig_SensorEnable_SelectedOnly;
		else {
			statusPtr->success = false;
			statusPtr->descrCode = AppStatusMessage_Descr_TelemetryConfig_UnknownValue_SensorsEnable;
			statusPtr->details = copyString(sensorsEnableJsonHandle->valuestring);
			return statusPtr;
		}
	}

	// 'sensors' element - mandatory
	cJSON *sensorsJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "sensors");
	if (sensorsJsonHandle == NULL || cJSON_GetArraySize(sensorsJsonHandle) < 1) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TelemetryConfig;
		statusPtr->details = copyString("sensors");
		return statusPtr;
	}
	/* one of these must be set
	 * for backwards compatibility: telemetryEventFrequency = eventFrequencyPerSec
	*/
	cJSON * numberOfEventsPerSecondJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "eventFrequencyPerSec");
	if(numberOfEventsPerSecondJsonHandle == NULL) numberOfEventsPerSecondJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "telemetryEventFrequency");
	if(numberOfEventsPerSecondJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TelemetryConfig;
		statusPtr->details = copyString("telemetryEventFrequency and eventFrequencyPerSec");
		return statusPtr;
	}
	// 'samplesPerEvent' - mandatory
	cJSON * numberOfSamplesPerEventJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "samplesPerEvent");
	if(numberOfSamplesPerEventJsonHandle == NULL) {
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_MissingMandatoryElement_TelemetryConfig;
		statusPtr->details = copyString("samplesPerEvent");
		return statusPtr;
	}
	// 'qos' - optional
	uint32_t qos = APP_RT_CFG_DEFAULT_QOS;
	cJSON * qosJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "qos");
	if(qosJsonHandle != NULL) qos = qosJsonHandle->valueint;
	if(qos!=0 && qos!=1) {
		qos = APP_RT_CFG_DEFAULT_QOS;
		statusPtr->success = false;
		statusPtr->descrCode = AppStatusMessage_Descr_TelemetryConfig_UnknownValue_QoS;
		statusPtr->details = copyString("qos");
		return statusPtr;
	}
	// 'payloadFormat' element - optional
	AppRuntimeConfig_Telemetry_PayloadFormat_T payloadFormat = APP_RT_CFG_DEFAULT_PAYLOAD_FORMAT;
	cJSON * payloadFormtJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "payloadFormat");
	if(payloadFormtJsonHandle != NULL) {
		if(NULL != strstr(payloadFormtJsonHandle->valuestring, APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR) ) {
			payloadFormat = AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Verbose;
		}
		else if(NULL != strstr(payloadFormtJsonHandle->valuestring, APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_COMPACT_STR) ) {
			payloadFormat = AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Compact;
		}
		else {
			statusPtr->success = false;
			statusPtr->descrCode = AppStatusMessage_Descr_TelemetryConfig_UnknownValue_PayloadFormat;
			statusPtr->details = copyString(payloadFormtJsonHandle->valuestring);
			return statusPtr;
		}
	}

	// populate
	configPtr->received.timestampStr = timestampStr;
	configPtr->received.delay2ApplyConfigSeconds = delaySeconds;
	configPtr->received.applyFlag = applyFlag;
	configPtr->received.activateAtBootTime = activateAtBootTime;
	configPtr->received.sensorEnableFlag = sensorsEnableFlag;
	configPtr->received.tagsJsonHandle = cJSON_Duplicate(tagsJsonHandle, true);
	configPtr->received.exchangeIdStr = exchangeIdStr;

	// enable/disable sensor capture
	int sensorCount = cJSON_GetArraySize(sensorsJsonHandle);
	for (int i = 0; i < sensorCount; i++) {
		cJSON* sensor = cJSON_GetArrayItem(sensorsJsonHandle, i);
		const char *actSensor = sensor->valuestring;
		if (strcmp(actSensor, "light") == 0) configPtr->received.sensors.isLight = true;
		else if (strcmp(actSensor, "accelerator") == 0) configPtr->received.sensors.isAccelerator = true;
		else if (strcmp(actSensor, "gyroscope") == 0) configPtr->received.sensors.isGyro = true;
		else if (strcmp(actSensor, "magnetometer") == 0) configPtr->received.sensors.isMagneto = true;
		else if (strcmp(actSensor, "humidity") == 0) configPtr->received.sensors.isHumidity = true;
		else if (strcmp(actSensor, "temperature") == 0) configPtr->received.sensors.isTemperature = true;
		actSensor = NULL;
	}
	configPtr->received.numberOfEventsPerSecond = numberOfEventsPerSecondJsonHandle->valueint;
	configPtr->received.numberOfSamplesPerEvent = numberOfSamplesPerEventJsonHandle->valueint;
	configPtr->received.qos = qos;
	configPtr->received.payloadFormat = payloadFormat;

	AppRuntimeConfigStatus_T * calcStatusPtr = appRuntimeConfig_CalculateAndValidateTelemetryRTParams(configPtr->received.numberOfSamplesPerEvent, configPtr->received.numberOfEventsPerSecond, rtParamsPtr);
	if(!calcStatusPtr->success) return calcStatusPtr;
	AppRuntimeConfig_DeleteStatus(calcStatusPtr);

	if(appRuntimeConfig_isEnabled) {
		appRuntimeConfig_ValidateTelemetryQueueSize(rtParamsPtr->numberOfSamplesPerEvent, &(configPtr->received.sensors), configPtr->received.payloadFormat, statusPtr);
		if(!statusPtr->success) return statusPtr;
	}

#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - appRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON: new & validated runtime config: \r\n");
	printf("[INFO] - targetTelemetryConfig: \r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_targetTelemetryConfig, configPtr);
	printf("[INFO] - activeTelemetryRTParams: \r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_activeTelemetryRTParams, rtParamsPtr);
#endif

	return statusPtr;
}
/**
 * @brief Validates and creates a new runtime configuration from the json.
 * @param[in] jsonHandle: the json containing all runtime configuration elements
 * @param[in,out] newConfigPtrPtr: the created and populated runtime configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_CreateValidatedAppRuntimeConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_T ** newConfigPtrPtr) {

	assert(jsonHandle);
	assert(newConfigPtrPtr);

	AppRuntimeConfigStatus_T * status = NULL;
	cJSON * currentJsonHandle = NULL;

	AppRuntimeConfig_T * newConfigPtr = appRuntimeConfig_CreateAppRuntimeConfig(false);

	currentJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "targetTelemetryConfig");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "targetTelemetryConfig");
	currentJsonHandle = cJSON_GetObjectItem(currentJsonHandle, "received");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "targetTelemetryConfig.received");
	status = appRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(currentJsonHandle, newConfigPtr->targetTelemetryConfigPtr, newConfigPtr->activeTelemetryRTParamsPtr);
	if(!status->success) return status;

	currentJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "statusConfig");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "statusConfig");
	currentJsonHandle = cJSON_GetObjectItem(currentJsonHandle, "received");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "statusConfig.received");
	status = appRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(currentJsonHandle, newConfigPtr->statusConfigPtr);
	if(!status->success) return status;

	currentJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "mqttBrokerConnectionConfig");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "mqttBrokerConnectionConfig");
	currentJsonHandle = cJSON_GetObjectItem(currentJsonHandle, "received");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "mqttBrokerConnectionConfig.received");
	status = appRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(currentJsonHandle, newConfigPtr->mqttBrokerConnectionConfigPtr);
	if(!status->success) return status;

	currentJsonHandle = cJSON_GetObjectItem((cJSON*)jsonHandle, "topicConfig");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "topicConfig");
	currentJsonHandle = cJSON_GetObjectItem(currentJsonHandle, "received");
	if(NULL == currentJsonHandle) return AppRuntimeConfig_CreateStatus(false, AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, "topicConfig.received");
	status = appRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(currentJsonHandle, newConfigPtr->topicConfigPtr);
	if(!status->success) return status;

	*newConfigPtrPtr = newConfigPtr;

	return status;

}
/**
 * @brief Calculates, validates, and creates the new telemetry runtime parameters.
 * @param[in] numberOfSamplesPerEvent: the number of samples per event
 * @param[in] numberOfEventsPerSecond: the number of events per second
 * @param[in,out] rtParamsPtr: the created and populated telemetry runtime paramters configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
static AppRuntimeConfigStatus_T * appRuntimeConfig_CalculateAndValidateTelemetryRTParams(const uint8_t numberOfSamplesPerEvent, const uint8_t numberOfEventsPerSecond, AppRuntimeConfig_TelemetryRTParams_T * rtParamsPtr) {

	assert(rtParamsPtr);

	AppRuntimeConfigStatus_T * statusPtr  = AppRuntimeConfig_CreateNewStatus();
	statusPtr->success = true;

	uint32_t publishPeriodcityMillis = 1000 / numberOfEventsPerSecond;

	uint32_t samplingPeriodicityMillis = publishPeriodcityMillis / numberOfSamplesPerEvent;

	AppRuntimeConfig_TelemetryRTParams_T * newRtParamsPtr = appRuntimeConfig_DuplicateTelemetryRTParams(rtParamsPtr);
	newRtParamsPtr->numberOfSamplesPerEvent = numberOfSamplesPerEvent;
	newRtParamsPtr->publishPeriodcityMillis = publishPeriodcityMillis;
	newRtParamsPtr->samplingPeriodicityMillis = samplingPeriodicityMillis;

	appRuntimeConfig_ValidateTelemetryRTParams(newRtParamsPtr, statusPtr);

	appRuntimeConfig_DeleteTelemetryRTParams(newRtParamsPtr);

	if(statusPtr->success) {
		rtParamsPtr->numberOfSamplesPerEvent = numberOfSamplesPerEvent;
		rtParamsPtr->publishPeriodcityMillis = publishPeriodcityMillis;
		rtParamsPtr->samplingPeriodicityMillis = samplingPeriodicityMillis;
	}
	return statusPtr;
}
/**
 * @brief Initialize the module.
 *
 * @param[in] deviceId : the device Id. Module takes a copy.
 *
 * @return Retcode_T : RETCODE_OK
 * @return Retcode_T : RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE)
 */
Retcode_T AppRuntimeConfig_Init(const char * deviceId) {

	assert(deviceId);

	Retcode_T retcode = RETCODE_OK;

	appRuntimeConfig_DeviceId = copyString(deviceId);

	appRuntimeConfigPtr_SemaphoreHandle = xSemaphoreCreateBinary();
	if(appRuntimeConfigPtr_SemaphoreHandle == NULL) return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE);
	xSemaphoreGive(appRuntimeConfigPtr_SemaphoreHandle);

	return retcode;

}
/**
 * @brief Setup the module.
 * @details
 * Reads the runtime configuration from file on SD card, or uses default settings if not found.<br/>
 * Validates and creates the runtime configuration and sets it.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_INVALID_DEFAULT_TELEMETRY_RT_PARAMS)
 */
Retcode_T AppRuntimeConfig_Setup(void) {

	Retcode_T retcode = RETCODE_OK;

	AppRuntimeConfig_T * newConfigPtr = NULL;

	cJSON * jsonHandle = NULL;
	if(RETCODE_OK == retcode) jsonHandle = appRuntimeConfig_ReadRuntimeConfigFromFile();
	bool useDefaults = (jsonHandle==NULL);

	if(jsonHandle != NULL) {
		// we have a runtime config JSON
		#ifdef DEBUG_APP_RUNTIME_CONFIG
		printf("[INFO] - AppRuntimeConfig_Setup: validating & creating runtime config ...\r\n");
		#endif

		AppRuntimeConfigStatus_T * statusPtr = appRuntimeConfig_CreateValidatedAppRuntimeConfigFromJSON(jsonHandle, &newConfigPtr);
		if(!statusPtr->success) {

			if(NULL != newConfigPtr) appRuntimeConfig_Delete(newConfigPtr);

			useDefaults = true;

			AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Warning, AppStatusMessage_Descr_InvalidPersistedRuntimeConfig_WillDelete, NULL);
			AppStatus_AddStatusItem(msg, "status", appRuntimeConfig_GetStatusAsJsonObject(statusPtr));
			AppStatus_SendStatusMessage(msg);

			// delete the invalid config file
			appRuntimeConfig_DeleteRuntimeConfigFile();
		}
		AppRuntimeConfig_DeleteStatus(statusPtr);
	}

	if(useDefaults) {

		newConfigPtr = appRuntimeConfig_CreateAppRuntimeConfig(true);

		#ifdef DEBUG_APP_RUNTIME_CONFIG
		printf("[INFO] - AppRuntimeConfig_Setup: using default values.\r\n");
		appRuntimeConfig_Print(AppRuntimeConfig_Element_AppRuntimeConfig, newConfigPtr);
		#endif

		// calculate rt telemetry params
		AppRuntimeConfigStatus_T * statusPtr  = appRuntimeConfig_CalculateAndValidateTelemetryRTParams(
				newConfigPtr->targetTelemetryConfigPtr->received.numberOfSamplesPerEvent,
				newConfigPtr->targetTelemetryConfigPtr->received.numberOfEventsPerSecond,
				newConfigPtr->activeTelemetryRTParamsPtr);

		if(statusPtr->success) {
			// copy broker config from bootstrap config
			const AppXDK_MQTT_Connect_T * mqttBoostrapConnectInfoPtr = AppConfig_GetMqttConnectInfoPtr();
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.brokerUrl = copyString(mqttBoostrapConnectInfoPtr->brokerUrl);
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.brokerPort = mqttBoostrapConnectInfoPtr->brokerPort;
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.brokerUsername = copyString(mqttBoostrapConnectInfoPtr->username);
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.brokerPassword = copyString(mqttBoostrapConnectInfoPtr->password);
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.isCleanSession = mqttBoostrapConnectInfoPtr->isCleanSession;
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.keepAliveIntervalSecs = mqttBoostrapConnectInfoPtr->keepAliveIntervalSecs;
			newConfigPtr->mqttBrokerConnectionConfigPtr->received.isSecureConnection = AppConfig_GetIsMqttBrokerConnectionSecure();
			// copy topic config
			newConfigPtr->topicConfigPtr->received.baseTopic = copyString(AppConfig_GetBaseTopicStr());

		} else {

			#ifdef DEBUG_APP_RUNTIME_CONFIG
			printf("[ERROR] - AppRuntimeConfig_Setup: appRuntimeConfig_CalculateAndValidateTelemetryRTParams() failed for default values.\r\n");
			cJSON * statusJson = appRuntimeConfig_GetStatusAsJsonObject(statusPtr);
			printJSON(statusJson);
			cJSON_Delete(statusJson);
			#endif

			retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_INVALID_DEFAULT_TELEMETRY_RT_PARAMS);
			AppRuntimeConfig_DeleteStatus(statusPtr);
			return retcode;

		}
		AppRuntimeConfig_DeleteStatus(statusPtr);

	} // useDefaults

	AppRuntimeConfig_ApplyNewRuntimeConfig(AppRuntimeConfig_Element_AppRuntimeConfig, newConfigPtr);

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - AppRuntimeConfig_Setup: the final runtime config:\r\n");
	appRuntimeConfig_Print(AppRuntimeConfig_Element_AppRuntimeConfig, appRuntimeConfigPtr);
	#endif

	return retcode;
}
/**
 * @brief Apply/save the new configuration for the selected configuration element.
 * @details The old configuration is deleted and the new one is set.
 * If apply flag is set to #AppRuntimeConfig_Apply_Persistent, saves the new configuration to SD card.
 * Blocks access while running.
 *
 * @note The new config must already be validated, no checks performed here.
 *
 * @param[in] configElement : the config element type
 * @param[in] newConfigPtr : the pointer to the new config element of type 'configElement'. Caller must not delete or modify the contents of newConfigPtr.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref appRuntimeConfig_PersistRuntimeConfig()
 *
 * @exception Retcode_RaiseError: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_APP_RUNTIME_CONFIG_RECEIVED_INVALID_RT_TELEMETRY)
 */
Retcode_T AppRuntimeConfig_ApplyNewRuntimeConfig(const AppRuntimeConfig_ConfigElement_T configElement, void * newConfigPtr) {

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	Retcode_T retcode = RETCODE_OK;

	AppRuntimeConfig_Apply_T applyFlag = AppRuntimeConfig_Apply_Transient;

	switch(configElement) {
	case AppRuntimeConfig_Element_topicConfig: {
		appRuntimeConfig_DeleteTopicConfig(appRuntimeConfigPtr->topicConfigPtr);
		appRuntimeConfigPtr->topicConfigPtr = (AppRuntimeConfig_TopicConfig_T *) newConfigPtr;
		applyFlag = appRuntimeConfigPtr->topicConfigPtr->received.applyFlag;
	}
		break;
	case AppRuntimeConfig_Element_mqttBrokerConnectionConfig: {
		appRuntimeConfig_DeleteMqttBrokerConnectionConfig(appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr);
		appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr = (AppRuntimeConfig_MqttBrokerConnectionConfig_T *) newConfigPtr;
		applyFlag = appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr->received.applyFlag;
	}
		break;
	case AppRuntimeConfig_Element_statusConfig: {
		appRuntimeConfig_DeleteStatusConfig(appRuntimeConfigPtr->statusConfigPtr);
		appRuntimeConfigPtr->statusConfigPtr = (AppRuntimeConfig_StatusConfig_T *) newConfigPtr;
		applyFlag = appRuntimeConfigPtr->statusConfigPtr->received.applyFlag;
	}
		break;
	case AppRuntimeConfig_Element_activeTelemetryRTParams: {
		appRuntimeConfig_DeleteTelemetryRTParams(appRuntimeConfigPtr->activeTelemetryRTParamsPtr);
		appRuntimeConfigPtr->activeTelemetryRTParamsPtr = (AppRuntimeConfig_TelemetryRTParams_T *) newConfigPtr;
		// these are never persisted
		applyFlag = AppRuntimeConfig_Apply_Transient;
	}
		break;
	case AppRuntimeConfig_Element_targetTelemetryConfig: {

		appRuntimeConfig_DeleteTelemetryConfig(appRuntimeConfigPtr->targetTelemetryConfigPtr);

		appRuntimeConfigPtr->targetTelemetryConfigPtr = (AppRuntimeConfig_TelemetryConfig_T *) newConfigPtr;

		// now also set the RT params
		appRuntimeConfig_DeleteTelemetryRTParams(appRuntimeConfigPtr->activeTelemetryRTParamsPtr);
		appRuntimeConfigPtr->activeTelemetryRTParamsPtr = appRuntimeConfig_CreateTelemetryRTParams();

		AppRuntimeConfigStatus_T * statusPtr  = appRuntimeConfig_CalculateAndValidateTelemetryRTParams(
														appRuntimeConfigPtr->targetTelemetryConfigPtr->received.numberOfSamplesPerEvent,
														appRuntimeConfigPtr->targetTelemetryConfigPtr->received.numberOfEventsPerSecond,
														appRuntimeConfigPtr->activeTelemetryRTParamsPtr);
		if(!statusPtr->success) {
			// if this happens then the RT telemetry has not be validated beforehand
			// coding error
			Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_APP_RUNTIME_CONFIG_RECEIVED_INVALID_RT_TELEMETRY));
		}

		AppRuntimeConfig_DeleteStatus(statusPtr);

		applyFlag = appRuntimeConfigPtr->targetTelemetryConfigPtr->received.applyFlag;
	}
		break;
	case AppRuntimeConfig_Element_AppRuntimeConfig: {
		// only called from AppRuntimeConfig_Setup()
		assert(!appRuntimeConfigPtr);
		appRuntimeConfigPtr = (AppRuntimeConfig_T *) newConfigPtr;
		// don't per persist
		applyFlag = AppRuntimeConfig_Apply_Transient;
	}
		break;
	default: assert(0);
	}

	// now persist it if selected
	if(applyFlag == AppRuntimeConfig_Apply_Persistent) {
		retcode = appRuntimeConfig_PersistRuntimeConfig();
	}

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return retcode;
}
/**
 * @brief Enable the module.
 * @details
 * Validates the telemetry queue size. <br/>
 * Prerequisites: Sensors must be enabled. @ref AppTimestamp must be enabled.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_RT_CONFIG_PUBLISH_DATA_LENGTH_EXCEEDS_MAX_LENGTH)
 */
Retcode_T AppRuntimeConfig_Enable(void) {

	Retcode_T retcode = RETCODE_OK;

	AppRuntimeConfigStatus_T * statusPtr = AppRuntimeConfig_CreateNewStatus();

	appRuntimeConfig_ValidateTelemetryQueueSize(
			appRuntimeConfigPtr->activeTelemetryRTParamsPtr->numberOfSamplesPerEvent,
			&(appRuntimeConfigPtr->targetTelemetryConfigPtr->received.sensors),
			appRuntimeConfigPtr->targetTelemetryConfigPtr->received.payloadFormat,
			statusPtr);
	if(!statusPtr->success) {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_RT_CONFIG_PUBLISH_DATA_LENGTH_EXCEEDS_MAX_LENGTH);
	}

	// check if runtime config was created from default. if so, set the timestamp
	// note: can only be done after SNTP time has been received
	if(RETCODE_OK == retcode && appRuntimeConfigPtr->internalState.source == AppRuntimeConfig_ConfigSource_InternalDefaults) {

		AppTimestamp_T ts = AppTimestamp_GetTimestamp(xTaskGetTickCount());

		appRuntimeConfigPtr->mqttBrokerConnectionConfigPtr->received.timestampStr = AppTimestamp_CreateTimestampStr(ts);
		appRuntimeConfigPtr->topicConfigPtr->received.timestampStr = AppTimestamp_CreateTimestampStr(ts);
		appRuntimeConfigPtr->statusConfigPtr->received.timestampStr = AppTimestamp_CreateTimestampStr(ts);
		appRuntimeConfigPtr->targetTelemetryConfigPtr->received.timestampStr = AppTimestamp_CreateTimestampStr(ts);
	}

	AppRuntimeConfig_DeleteStatus(statusPtr);
	if(RETCODE_OK == retcode) appRuntimeConfig_isEnabled = true;

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	if(RETCODE_OK == retcode) {
		printf("[INFO] - AppRuntimeConfig_Enable: the boot runtime config:\r\n");
		appRuntimeConfig_Print(AppRuntimeConfig_Element_AppRuntimeConfig, appRuntimeConfigPtr);
	}
	#endif

	return retcode;
}
/**
 * @brief Sends the runtime configuration as multiple parts (each element as one part) using @ref AppStatus_SendStatusMessagePart() adding the exchangeId.
 * Use to send the configuration as response to a command.
 * @param[in] jsonHandle: the configuration json. Is deleted here.
 * @param[in] exchangeId: added to the message
 *
 */
void appRuntimeConfig_SendConfig(cJSON * jsonHandle, const char * exchangeId) {

	assert(jsonHandle);
	assert(exchangeId);

	uint8_t totalNumParts = 5;

	AppStatus_SendStatusMessagePart(AppStatusMessage_Descr_PersistedRuntimeConfig_Header, NULL, exchangeId, totalNumParts, 0, NULL, NULL);

	cJSON * topicConfigJsonHandle =cJSON_GetObjectItem(jsonHandle, "topicConfig");
	if(topicConfigJsonHandle==NULL) AppStatus_SendStatusMessage(AppStatus_CreateMessageWithExchangeId(AppStatusMessage_Status_Error, AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoTopicConfigFound, NULL, exchangeId));
	else AppStatus_SendStatusMessagePart(AppStatusMessage_Descr_PersistedRuntimeConfig_TopicConfig, NULL, exchangeId, totalNumParts, 1, "persistedRuntimeConfig.topicConfig", cJSON_Duplicate(topicConfigJsonHandle, true));

	cJSON * mqttBrokerConnectionConfigJsonHandle =cJSON_GetObjectItem(jsonHandle, "mqttBrokerConnectionConfig");
	if(mqttBrokerConnectionConfigJsonHandle==NULL) AppStatus_SendStatusMessage(AppStatus_CreateMessageWithExchangeId(AppStatusMessage_Status_Error, AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoMqttBrokerConnectionConfigFound, NULL, exchangeId));
	else AppStatus_SendStatusMessagePart(AppStatusMessage_Descr_PersistedRuntimeConfig_MqttBrokerConnectionConfig, NULL, exchangeId, totalNumParts, 2, "persistedRuntimeConfig.mqttBrokerConnectionConfig", cJSON_Duplicate(mqttBrokerConnectionConfigJsonHandle, true));

	cJSON * statusConfigJsonHandle =cJSON_GetObjectItem(jsonHandle, "statusConfig");
	if(statusConfigJsonHandle==NULL) AppStatus_SendStatusMessage(AppStatus_CreateMessageWithExchangeId(AppStatusMessage_Status_Error, AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoStatusConfigFound,NULL, exchangeId));
	else AppStatus_SendStatusMessagePart(AppStatusMessage_Descr_PersistedRuntimeConfig_StatusConfig, NULL, exchangeId, totalNumParts, 3, "persistedRuntimeConfig.statusConfig", cJSON_Duplicate(statusConfigJsonHandle, true));

	cJSON * targetTelemetryConfigJsonHandle =cJSON_GetObjectItem(jsonHandle, "targetTelemetryConfig");
	if(targetTelemetryConfigJsonHandle==NULL) AppStatus_SendStatusMessage(AppStatus_CreateMessageWithExchangeId(AppStatusMessage_Status_Error, AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoTargetTelemetryConfigFound, NULL, exchangeId));
	else AppStatus_SendStatusMessagePart(AppStatusMessage_Descr_PersistedRuntimeConfig_TargetTelemetryConfig, NULL, exchangeId, totalNumParts, 4, "persistedRuntimeConfig.targetTelemetryConfig", cJSON_Duplicate(targetTelemetryConfigJsonHandle, true));

	cJSON_Delete(jsonHandle);

}
/**
 * @brief Sends the active runtime configuration using @ref appRuntimeConfig_SendConfig().
 * Use as response to a command.
 * @param[in] exchangeId: the exchangeId to add to the messages
 */
void AppRuntimeConfig_SendActiveConfig(const char * exchangeId) {

	assert(exchangeId);

	appRuntimeConfig_SendConfig(AppRuntimeConfig_GetAsJsonObject(AppRuntimeConfig_Element_AppRuntimeConfig), exchangeId);

}
/**
 * @brief Sends the persisted runtime configuration file using @ref appRuntimeConfig_SendConfig() or a status message if not found using @ref AppStatus_SendStatusMessage().
 * Use as response to a command.
 * @param[in] exchangeId: the exchangeId to add to the if file not found
 */
void AppRuntimeConfig_SendFile(const char * exchangeId) {

	assert(exchangeId);

	cJSON * jsonHandle =  appRuntimeConfig_ReadRuntimeConfigFromFile();

	#ifdef DEBUG_APP_RUNTIME_CONFIG
	printf("[INFO] - AppRuntimeConfig_SendFile: the persisted runtime config json file:\r\n");
	if(jsonHandle != NULL) printJSON(jsonHandle);
	else printf("[INFO] - AppRuntimeConfig_SendFile: not found.\r\n");
	#endif

	if(NULL != jsonHandle) {
		appRuntimeConfig_SendConfig(jsonHandle, exchangeId);
	} else {
		AppStatus_SendStatusMessage(AppStatus_CreateMessageWithExchangeId(AppStatusMessage_Status_Info, AppStatusMessage_Descr_PersistedRuntimeConfig_NotFound, NULL, exchangeId));
	}

}
/**
 * @brief Deletes the runtime config file on the SD card.
 */
void AppRuntimeConfig_DeleteFile(void) { appRuntimeConfig_DeleteRuntimeConfigFile(); }
/**
 * @brief Persist the current active runtime config to the SD card.
 * @return Retcode_T: retcode from @ref appRuntimeConfig_PersistRuntimeConfig()
 */
Retcode_T AppRuntimeConfig_PersistRuntimeConfig2File(void) {

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	Retcode_T retcode = appRuntimeConfig_PersistRuntimeConfig();

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return retcode;
}
/**
 * @brief Adapts the telemetry and sampling rate to meanPublishTicks.
 * @note Algorithm / logic needs revisiting.
 * @param[in] meanPublishTicks: the mean number of ticks measured for telemetry publish over a specific sampling size.
 * @return AppRuntimeConfig_TelemetryRTParams_T *: the new calculated runtime telemetry parameters.
 */
AppRuntimeConfig_TelemetryRTParams_T * AppRuntimeConfig_AdaptTelemetryRateDown(uint32_t meanPublishTicks) {

/*
	1) check the last time telemetry was adapted
		- space it for at least 5 seconds
	2) discard if
		- only 1 tick apart?
		- but apply if 5 x in a row 1 tick apart in 5 seconds
*/
	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfig_TelemetryRTParams_T * newRtParamsPtr = appRuntimeConfig_DuplicateTelemetryRTParams(appRuntimeConfigPtr->activeTelemetryRTParamsPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	// adapt the publish frequency
	int32_t difference = meanPublishTicks - newRtParamsPtr->publishPeriodcityMillis;
	if(difference > 0) {

		uint32_t newPublishPeriodcityMillis = newRtParamsPtr->publishPeriodcityMillis + (difference/2) + 1;
		//uint32_t newPublishPeriodcityMillis = newrtParamsPtr->publishPeriodcityMillis + difference;

		uint32_t newSamplingPeriodicityMillis = newPublishPeriodcityMillis / newRtParamsPtr->numberOfSamplesPerEvent;

		newRtParamsPtr->publishPeriodcityMillis = newPublishPeriodcityMillis;
		newRtParamsPtr->samplingPeriodicityMillis = newSamplingPeriodicityMillis;
	}

	return newRtParamsPtr;
}
/**
 * @brief Validates the mqtt broker configuration contained in jsonHandle and returns the new configuration.
 * Blocks access while in progress.
 * @param[in] jsonHandle: the JSON containing the new mqtt broker configuration
 * @param[in,out] configPtr: the populated mqtt broker configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfigStatus_T * statusPtr =  appRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(jsonHandle, configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return statusPtr;
}
/**
 * @brief Validates the topic configuration contained in jsonHandle and returns the new topic configuration.
 * Blocks access while in progress.
 * @param[in] jsonHandle: the JSON containing the new topic configuration
 * @param[in,out] configPtr: the populated topic configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TopicConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfigStatus_T * statusPtr =  appRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(jsonHandle, configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return statusPtr;
}
/**
 * @brief Validates the status configuration contained in jsonHandle and returns the new configuration.
 * Blocks access while in progress.
 * @param[in] jsonHandle: the JSON containing the new status configuration
 * @param[in,out] configPtr: the populated status configuration
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_StatusConfig_T * configPtr) {

	assert(jsonHandle);
	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfigStatus_T * statusPtr =  appRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(jsonHandle, configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return statusPtr;
}
/**
 * @brief Validates the telemetry configuration contained in jsonHandle and returns the new configuration as well as the runtime parameters.
 * Blocks access while in progress.
 * @param[in] jsonHandle: the JSON containing the new telemetry configuration
 * @param[in,out] configPtr: the populated telemetry configuration
 * @param[in,out] rtParamsPtr: the new telemetry runtime parameters
 * @return AppRuntimeConfigStatus_T *: the status of the operation
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TelemetryConfig_T * configPtr, AppRuntimeConfig_TelemetryRTParams_T * rtParamsPtr) {

	assert(jsonHandle);
	assert(configPtr);

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	AppRuntimeConfigStatus_T * statusPtr =  appRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(jsonHandle, configPtr, rtParamsPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();

	return statusPtr;
}
/**
 * @brief Create a new empty status object.
 * @return AppRuntimeConfigStatus_T *: the new status object
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_CreateNewStatus(void) {
	AppRuntimeConfigStatus_T * statusPtr = malloc(sizeof(AppRuntimeConfigStatus_T));
	statusPtr->success = false;
	statusPtr->descrCode = AppStatusMessage_Descr_NULL;
	statusPtr->details = NULL;
	return statusPtr;
}
/**
 * @brief Create a new, populated status object.
 * @param[in] success: true/false
 * @param[in] descrCode: the description code
 * @param[in] details: the details string
 * @return AppRuntimeConfigStatus_T *: the new status object
 */
AppRuntimeConfigStatus_T * AppRuntimeConfig_CreateStatus(const bool success, const AppStatusMessage_DescrCode_T descrCode, const char * details) {
	AppRuntimeConfigStatus_T * statusPtr = malloc(sizeof(AppRuntimeConfigStatus_T));
	statusPtr->success = success;
	statusPtr->descrCode = descrCode;
	statusPtr->details = copyString(details);
	return statusPtr;
}
/**
 * @brief Delete a status object.
 * @param[in] statusPtr: the status to delete
 */
void AppRuntimeConfig_DeleteStatus(AppRuntimeConfigStatus_T * statusPtr) {
	free(statusPtr->details);
	free(statusPtr);
}
/**
 * @brief Return the status as a JSON object.
 * @param[in] statusPtr: the status
 * @return cJSON *: the created JSON object
 */
static cJSON * appRuntimeConfig_GetStatusAsJsonObject(const AppRuntimeConfigStatus_T * statusPtr) {
	cJSON * jsonHandle = cJSON_CreateObject();
	cJSON_AddBoolToObject(jsonHandle, "success", statusPtr->success);
	cJSON_AddNumberToObject(jsonHandle, "descrCode", statusPtr->descrCode);
	cJSON_AddItemToObject(jsonHandle, "details", cJSON_CreateString(statusPtr->details));
	return jsonHandle;
}

#if defined(DEBUG_APP_CONTROLLER) || defined(DEBUG_APP_RUNTIME_CONFIG)
/**
 * @brief Print the configPtr to the console as JSON object.
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_PrintTargetTelemetryConfig(const AppRuntimeConfig_TelemetryConfig_T * configPtr) {
	printf("--------------------------------------------------------------------------\r\n");
	printf("appRuntimeConfig_PrintTargetTelemetryConfig:\r\n");
	printf("--------------------------------------------------------------------------\r\n");

	cJSON * configJson = appRuntimeConfig_GetTargetTelemetryConfigAsJsonObject(configPtr);
	printJSON(configJson);
	cJSON_Delete(configJson);

	printf("--------------------------------------------------------------------------\r\n");
}
/**
 * @brief Print the configPtr to the console as JSON object.
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_PrintActiveTelemetryRTParams(const AppRuntimeConfig_TelemetryRTParams_T * configPtr) {
	printf("--------------------------------------------------------------------------\r\n");
	printf("appRuntimeConfig_PrintActiveTelemetryRTParams:\r\n");
	printf("--------------------------------------------------------------------------\r\n");

	cJSON * configJson = appRuntimeConfig_GetActiveTelemetryRTParamsAsJsonObject(configPtr);
	printJSON(configJson);
	cJSON_Delete(configJson);

	printf("--------------------------------------------------------------------------\r\n");
}
/**
 * @brief Print the configPtr to the console as JSON object.
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_PrintStatusConfig(const AppRuntimeConfig_StatusConfig_T *configPtr) {
	printf("--------------------------------------------------------------------------\r\n");
	printf("appRuntimeConfig_PrintStatusConfig:\r\n");
	printf("--------------------------------------------------------------------------\r\n");

	cJSON * configJson = appRuntimeConfig_GetStatusConfigAsJsonObject(configPtr);
	printJSON(configJson);
	cJSON_Delete(configJson);

	printf("--------------------------------------------------------------------------\r\n");
}
/**
 * @brief Print the configPtr to the console as JSON object.
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_PrintMqttBrokerConnectionConfig(const AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr) {
	printf("--------------------------------------------------------------------------\r\n");
	printf("appRuntimeConfig_PrintMqttBrokerConnectionConfig:\r\n");
	printf("--------------------------------------------------------------------------\r\n");

	cJSON * configJson = appRuntimeConfig_GetMqttBrokerConnectionConfigAsJsonObject(configPtr);
	printJSON(configJson);
	cJSON_Delete(configJson);

	printf("--------------------------------------------------------------------------\r\n");
}
/**
 * @brief Print the configPtr to the console as JSON object.
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_PrintTopicConfig(const AppRuntimeConfig_TopicConfig_T * configPtr) {
	printf("--------------------------------------------------------------------------\r\n");
	printf("appRuntimeConfig_PrintTopicConfig:\r\n");
	printf("--------------------------------------------------------------------------\r\n");

	cJSON * configJson = appRuntimeConfig_GetTopicConfigAsJsonObject(configPtr);
	printJSON(configJson);
	cJSON_Delete(configJson);

	printf("--------------------------------------------------------------------------\r\n");
}
/**
 * @brief Print the configElement contained in the configPtr to the console as JSON object.
 * @param[in] configElement: which element the configPtr contains
 * @param[in] configPtr: the config to print
 */
static void appRuntimeConfig_Print(AppRuntimeConfig_ConfigElement_T configElement, const void * configPtr) {

	switch(configElement) {
	case AppRuntimeConfig_Element_topicConfig:
		appRuntimeConfig_PrintTopicConfig((AppRuntimeConfig_TopicConfig_T *) configPtr);
		break;
	case AppRuntimeConfig_Element_mqttBrokerConnectionConfig:
		appRuntimeConfig_PrintMqttBrokerConnectionConfig((AppRuntimeConfig_MqttBrokerConnectionConfig_T *) configPtr);
		break;
	case AppRuntimeConfig_Element_statusConfig:
		appRuntimeConfig_PrintStatusConfig((AppRuntimeConfig_StatusConfig_T *) configPtr);
		break;
	case AppRuntimeConfig_Element_activeTelemetryRTParams:
		appRuntimeConfig_PrintActiveTelemetryRTParams((AppRuntimeConfig_TelemetryRTParams_T *) configPtr);
		break;
	case AppRuntimeConfig_Element_targetTelemetryConfig:
		appRuntimeConfig_PrintTargetTelemetryConfig((AppRuntimeConfig_TelemetryConfig_T *) configPtr);
		break;
	case AppRuntimeConfig_Element_AppRuntimeConfig: {
		printf("--------------------------------------------------------------------------\r\n");
		printf("AppRuntimeConfig:\r\n");
		printf("--------------------------------------------------------------------------\r\n");
		AppRuntimeConfig_T * appRTconfigPtr = (AppRuntimeConfig_T * ) configPtr;
		appRuntimeConfig_PrintTopicConfig(appRTconfigPtr->topicConfigPtr);
		appRuntimeConfig_PrintMqttBrokerConnectionConfig(appRTconfigPtr->mqttBrokerConnectionConfigPtr);
		appRuntimeConfig_PrintStatusConfig(appRTconfigPtr->statusConfigPtr);
		appRuntimeConfig_PrintTargetTelemetryConfig(appRTconfigPtr->targetTelemetryConfigPtr);
		appRuntimeConfig_PrintActiveTelemetryRTParams(appRTconfigPtr->activeTelemetryRTParamsPtr);
		printf("--------------------------------------------------------------------------\r\n");
	}
		break;
	default: assert(0);
	}
}
/**
 * @brief Print the configElement contained in the configPtr to the console as JSON object.
 * Blocks access while in progress.
 * @param[in] configElement: which element the configPtr contains
 * @param[in] configPtr: the config to print
 */
void AppRuntimeConfig_Print(AppRuntimeConfig_ConfigElement_T configElement, const void * configPtr) {

	appRuntimeConfig_BlockAccess2AppRuntimeConfigPtr();

	appRuntimeConfig_Print(configElement, configPtr);

	appRuntimeConfig_AllowAccess2AppRuntimeConfigPtr();
}
#endif

/**@} */
/** ************************************************************************* */










