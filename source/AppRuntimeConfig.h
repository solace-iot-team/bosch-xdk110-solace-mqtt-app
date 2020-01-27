/*
 * AppRuntimeConfig.h
 *
 *  Created on: 23 Jul 2019
 *      Author: rjgu
 */
/**
 * @ingroup AppRuntimeConfig
 * @{
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 **/
#ifndef SOURCE_APPRUNTIMECONFIG_H_
#define SOURCE_APPRUNTIMECONFIG_H_

#include "XdkAppInfo.h"

#include "cJSON.h"
#include "BCDS_CmdProcessor.h"

/* default options */
#define APP_RT_CFG_DEFAULT_APPLY						AppRuntimeConfig_Apply_Transient /**< default apply flag */
#define APP_RT_CFG_DEFAULT_ACTIVATE_AT_BOOT_TIME		true /**< default for activating telemetry at boot time */
#define APP_RT_CFG_DEFAULT_SENSOR_ENABLE				AppRuntimeConfig_SensorEnable_All /**< default for selecting which sensors to enable at boot time */
#define APP_RT_CFG_DEFAULT_DELAY_SECS					(UINT8_C(1)) /**< default for delay applying a command/configuration */
#define APP_RT_CFG_DEFAULT_TAGS_JSON					"{\"mode\": \"APP_DEFAULT_MODE\"}" /**< default for tags object */
#define APP_RT_CFG_DEFAULT_EXCHANGE_ID					"default-exchange-id" /**< default exchange id */

#define APP_RT_CFG_DEFAULT_NUM_EVENTS_PER_SEC			(UINT8_C(1)) /**< default number of events per second */
#define APP_RT_CFG_DEFAULT_NUM_SAMPLES_PER_EVENT		(UINT8_C(1)) /**< default number of samples per event */
#define APP_RT_CFG_DEFAULT_QOS							(UINT32_C(0)) /**< default qos */
#define APP_RT_CFG_DEFAULT_PAYLOAD_FORMAT				AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Verbose /**< default payload format */
#define APP_RT_CFG_DEFAULT_PUBLISH_PERIODICITY_MILLIS	(UINT32_C(1000)) /**< default publish period in millis. must match #APP_RT_CFG_DEFAULT_NUM_EVENTS_PER_SEC */
#define APP_RT_CFG_DEFAULT_SAMPLING_PERIODICITY_MILLIS	(UINT32_C(1000)) /**< default sampling period in millis. must match #APP_RT_CFG_DEFAULT_NUM_SAMPLES_PER_EVENT*/

#define APP_RT_CFG_DEFAULT_TOPIC_METHOD_CREATE			"CREATE" /**< topic default value for method create */
#define APP_RT_CFG_DEFAULT_TOPIC_METHOD_UPDATE			"UPDATE" /**< topic default value for method update */

/**
 * @brief Typedef for apply flag.
 */
typedef enum {
    AppRuntimeConfig_Apply_Transient = 0, /**< transient */
	AppRuntimeConfig_Apply_Persistent /**< persistent */
} AppRuntimeConfig_Apply_T;

#define APP_RT_CFG_APPLY_TRANSIENT					"TRANSIENT" /**< apply value transient in json */
#define APP_RT_CFG_APPLY_PERSISTENT					"PERSISTENT" /**< apply value persistent in json */
/**
 * @brief Typedef for sensor activation at boot time.
 */
typedef enum {
	AppRuntimeConfig_SensorEnable_All = 0,
	AppRuntimeConfig_SensorEnable_SelectedOnly
} AppRuntimeConfig_SensorEnable_T;

#define APP_RT_CFG_SENSORS_ENABLE_ALL				"ALL"			/**< value to enable all sensors in json*/
#define APP_RT_CFG_SENSORS_ENABLE_SELECTED			"SELECTED"		/**< value to enable only requested sensors in json*/
/**
 * @brief Typedef for source of configuration.
 */
typedef enum {
	AppRuntimeConfig_ConfigSource_InternalDefaults = 0,
	AppRuntimeConfig_ConfigSource_ExternallySet
} AppRuntimeConfig_ConfigSource_T;
/**
 * @brief Typedef for status responses.
 */
typedef struct {
	bool success; /**< success or not */
	AppStatusMessage_DescrCode_T descrCode; /**< the description code */
	char * details; /**< any details */
} AppRuntimeConfigStatus_T;

/**
 * @brief Typedef for topic config.
 */
typedef struct {
	struct {
		char * timestampStr; /**< the timestamp when config was received */
		char * exchangeIdStr; /**< the exchangeId from the original command */
		cJSON * tagsJsonHandle; /**< the tags object */
        uint8_t delay2ApplyConfigSeconds; /**< delay in seconds */
        AppRuntimeConfig_Apply_T applyFlag; /**< how to apply it */
        char * baseTopic; /**< the base topic string */
        char * methodCreate; /**< the create method topic element */
        char * methodUpdate; /**< the update method topic element */
	} received; /**< the received config */
} AppRuntimeConfig_TopicConfig_T;
/**
 * @brief Typedef for mqtt broker config.
 */
typedef struct {
	struct {
		char * timestampStr; /**< the timestamp when config was received */
		char * exchangeIdStr; /**< the exchangeId from the original command */
		cJSON * tagsJsonHandle; /**< the tags object */
        uint8_t delay2ApplyConfigSeconds; /**< delay in seconds */
        AppRuntimeConfig_Apply_T applyFlag;  /**< how to apply it */
        char * brokerUrl; /**< the broker URL */
        uint16_t brokerPort; /**< broker port */
        char * brokerUsername; /**< user name */
        char * brokerPassword; /**< password */
        bool isCleanSession; /**< clean session flag */
        bool isSecureConnection; /**< secure connection flag */
        uint32_t keepAliveIntervalSecs; /**< connection keep alive in seconds */
	} received;  /**< the received config */
} AppRuntimeConfig_MqttBrokerConnectionConfig_T;
/**
 * @brief Typedef for defining which periodic status message to send.
 */
typedef enum {
	AppRuntimeConfig_PeriodicStatusType_Full = 0,
	AppRuntimeConfig_PeriodicStatusType_Short,
} AppRuntimeConfig_PeriodicStatusType_T;

#define APP_RT_CFG_STATUS_PERIODIC_TYPE_FULL		"FULL_STATUS" /**< json value for full status */
#define APP_RT_CFG_STATUS_PERIODIC_TYPE_SHORT		"SHORT_STATUS" /**< json value for short status */

/**
 * @brief Typedef for status config.
 */
typedef struct {
	struct {
		char * timestampStr; /**< the timestamp when config was received */
		char * exchangeIdStr; /**< the exchangeId from the original command */
		cJSON * tagsJsonHandle; /**< the tags object */
        uint8_t delay2ApplyConfigSeconds; /**< delay in seconds */
        AppRuntimeConfig_Apply_T applyFlag;  /**< how to apply it */
		bool isSendPeriodicStatus; /**< flag to send periodic status events or not */
		AppRuntimeConfig_PeriodicStatusType_T periodicStatusType; /**< type of periodic status events */
	    uint32_t periodicStatusIntervalSecs; /**< interval of periodic status events in seconds */
	    uint32_t qos; /**< qos of status messages */
	} received; /**< the received config */
} AppRuntimeConfig_StatusConfig_T;

#define APP_RT_CFG_DEFAULT_STATUS_SEND_PERIODIC_FLAG			false /**< default for sending periodic status events */
#define APP_RT_CFG_DEFAULT_STATUS_INTERVAL_SECS					(UINT8_C(300)) /**< default status interval in seconds */
#define APP_RT_CFG_DEFAULT_STATUS_PERIODIC_TYPE					AppRuntimeConfig_PeriodicStatusType_Short /**< default type of status event */
#define APP_RT_CFG_DEFAULT_STATUS_QOS							(UINT32_C(0)) /**< default status event qos */
#define APP_RT_CFG_STATUS_MIN_INTERVAL_SECS						(UINT32_C(60)) /**< minimum interval (seconds) configuration for the recurring status */

/**
 * @brief Typedef for runtime telemetry parameters.
 */
typedef struct {
    uint32_t samplingPeriodicityMillis; /**< sampling interval in milli seconds */
    uint32_t publishPeriodcityMillis; /**< publishing interval in milli seconds */
    uint8_t numberOfSamplesPerEvent; /**< number of samples per event */
} AppRuntimeConfig_TelemetryRTParams_T;
/**
 * @brief Typedef for sensor selection configuration.
 */
typedef struct {
	bool isLight; /**< send light values*/
	bool isAccelerator; /**< send accelerometer values */
	bool isGyro; /**< send gyroscope values */
	bool isMagneto; /**< send magnetometer values */
	bool isHumidity; /**< send humidity values */
	bool isTemperature; /**< send temperature values */
	bool isPressure; /**< send pressure values */
} AppRuntimeConfig_Sensors_T;
/**
 * @brief Typedef for telemetry payload format
 */
typedef enum {
	AppRuntimeConfig_Telemetry_PayloadFormat_NULL = 0,
	AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Verbose,
	AppRuntimeConfig_Telemetry_PayloadFormat_V1_Json_Compact,
} AppRuntimeConfig_Telemetry_PayloadFormat_T;

#define APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR			"V1_JSON_VERBOSE" /**< json value for V1 json verbose payload format */
#define APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_COMPACT_STR			"V1_JSON_COMPACT" /**< json value for V1 json compact payload format */

/**
 * @brief Typedef telemetry config.
 */
typedef struct {
	struct {
		char * timestampStr; /**< the timestamp when config was received */
		char * exchangeIdStr; /**< the exchangeId from the original command */
		cJSON * tagsJsonHandle; /**< the tags object */
        uint8_t delay2ApplyConfigSeconds; /**< delay in seconds */
        AppRuntimeConfig_Apply_T applyFlag;  /**< how to apply it */
        bool activateAtBootTime; /**< flag if telemetry sampling/publishing should be activated at boot time or not */
        AppRuntimeConfig_SensorEnable_T sensorEnableFlag; /**< unused */
		uint8_t numberOfEventsPerSecond; /**< number of events per second to publish */
	    uint8_t numberOfSamplesPerEvent; /**< number of sensor samples per event */
	    AppRuntimeConfig_Sensors_T sensors; /**< which sensor values to send */
	    uint32_t qos; /**< the qos for telemetry events */
	    AppRuntimeConfig_Telemetry_PayloadFormat_T payloadFormat; /**< the payload format */
	} received;  /**< the received config */
} AppRuntimeConfig_TelemetryConfig_T;
/**
 * @brief Typedef for the runtime configuration.
 */
typedef struct {
	AppRuntimeConfig_TelemetryConfig_T * targetTelemetryConfigPtr; /**< telemetry config */
	AppRuntimeConfig_TelemetryRTParams_T * activeTelemetryRTParamsPtr; /**< currently active telemetry parameters. they can in theory be adjusted automatically */
	AppRuntimeConfig_StatusConfig_T * statusConfigPtr; /**< the status config */
	AppRuntimeConfig_MqttBrokerConnectionConfig_T * mqttBrokerConnectionConfigPtr; /**< the broker config */
	AppRuntimeConfig_TopicConfig_T * topicConfigPtr; /**< the topic config */
	struct {
		AppRuntimeConfig_ConfigSource_T source; /**< the source of the current config */
	} internalState; /**< structure to capture internal info */
} AppRuntimeConfig_T;
/**
 * @brief Type for the configuration elements.
 */
typedef enum {
	AppRuntimeConfig_Element_targetTelemetryConfig = 0, /**< telemetry config */
	AppRuntimeConfig_Element_activeTelemetryRTParams, /**< active telemetry runtime parameters */
	AppRuntimeConfig_Element_statusConfig, /**< status config */
	AppRuntimeConfig_Element_mqttBrokerConnectionConfig, /**< broker config */
	AppRuntimeConfig_Element_topicConfig, /**< topic config */
	AppRuntimeConfig_Element_AppRuntimeConfig /**< the entire runtime config */
} AppRuntimeConfig_ConfigElement_T;


const AppRuntimeConfig_T * getAppRuntimeConfigPtr(void);

cJSON * AppRuntimeConfig_GetAsJsonObject(const AppRuntimeConfig_ConfigElement_T configElement);

AppRuntimeConfig_TopicConfig_T * AppRuntimeConfig_CreateTopicConfig(void);

AppRuntimeConfig_TopicConfig_T * AppRuntimeConfig_DuplicateTopicConfig(const AppRuntimeConfig_TopicConfig_T * orgConfigPtr);

AppRuntimeConfig_Sensors_T * AppRuntimeConfig_DuplicateSensors(const AppRuntimeConfig_Sensors_T * orgSensorsPtr);

AppRuntimeConfig_MqttBrokerConnectionConfig_T * AppRuntimeConfig_CreateMqttBrokerConnectionConfig(void);

AppRuntimeConfig_StatusConfig_T * AppRuntimeConfig_CreateStatusConfig(void);

AppRuntimeConfig_TelemetryRTParams_T * AppRuntimeConfig_CreateTelemetryRTParams(void);

AppRuntimeConfig_TelemetryConfig_T * AppRuntimeConfig_CreateTelemetryConfig(void);

void AppRuntimeConfig_DeleteTelemetryRTParams(AppRuntimeConfig_TelemetryRTParams_T * configPtr);

void AppRuntimeConfig_DeleteTopicConfig(AppRuntimeConfig_TopicConfig_T * configPtr);

void AppRuntimeConfig_DeleteSensors(AppRuntimeConfig_Sensors_T * sensorsPtr);

Retcode_T AppRuntimeConfig_Init(const char * deviceId);

Retcode_T AppRuntimeConfig_Setup(void);

Retcode_T AppRuntimeConfig_ApplyNewRuntimeConfig(const AppRuntimeConfig_ConfigElement_T configElement, void * newConfigPtr);

Retcode_T AppRuntimeConfig_Enable(void);

void AppRuntimeConfig_SendActiveConfig(const char * exchangeId);

void AppRuntimeConfig_SendFile(const char * exchangeId);

void AppRuntimeConfig_DeleteFile(void);

Retcode_T AppRuntimeConfig_PersistRuntimeConfig2File(void);

AppRuntimeConfig_TelemetryRTParams_T * AppRuntimeConfig_AdaptTelemetryRateDown(uint32_t meanPublishTicks);

AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateMqttBrokerConnectionConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_MqttBrokerConnectionConfig_T * configPtr);

AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateTopicConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TopicConfig_T * configPtr);

AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateStatusConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_StatusConfig_T * configPtr);

AppRuntimeConfigStatus_T * AppRuntimeConfig_PopulateAndValidateTelemetryConfigFromJSON(const cJSON * jsonHandle, AppRuntimeConfig_TelemetryConfig_T * configPtr, AppRuntimeConfig_TelemetryRTParams_T * rtParamsPtr);

#ifdef DEBUG_APP_CONTROLLER
void AppRuntimeConfig_Print(AppRuntimeConfig_ConfigElement_T configElement, const void * configPtr);
#endif

AppRuntimeConfigStatus_T * AppRuntimeConfig_CreateNewStatus(void);

AppRuntimeConfigStatus_T * AppRuntimeConfig_CreateStatus(const bool success, const AppStatusMessage_DescrCode_T descrCode, const char * details);

void AppRuntimeConfig_DeleteStatus(AppRuntimeConfigStatus_T * statusPtr);


#endif /* SOURCE_APPRUNTIMECONFIG_H_ */

/**@} */
/** ************************************************************************* */
