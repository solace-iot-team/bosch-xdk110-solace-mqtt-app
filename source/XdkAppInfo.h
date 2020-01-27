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
 * @defgroup XdkAppInfo XdkAppInfo
 * @{
 *
 * @brief Defines the taks & command processor parameters and enums for ModuleIds, RETCODEs, Status codes, Status Descriptions, etc.
 *
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 **/


#ifndef XDK_APPINFO_H_
#define XDK_APPINFO_H_

#include "XdkCommonInfo.h"
#include "BCDS_Retcode.h"


#define SOLACE_APP_PACKAGE_DESCR	"SOLACE-APP" /**< the app package description for Retcodes */
#define SOLACE_APP_PACKAGE_ID		55			/**< the app package id for Retcodes */
#undef BCDS_PACKAGE_ID
#define BCDS_PACKAGE_ID SOLACE_APP_PACKAGE_ID	/**< set the package id to the solace app package id for retcodes */

/**
 * @defgroup ProcessorAndTasks Processor And Task Priorities & Parameters
 * @{
 *
 * @brief Command processor and task priorities
 *
 */
#define APP_TELEMETRY_SAMPLING_TASK_PRIORITY		(UINT32_C(1)) /**< APP_TELEMETRY_SAMPLING_TASK_PRIORITY */

#define APP_TELEMETRY_PUBLISHING_TASK_PRIORITY		(UINT32_C(2)) /**< APP_TELEMETRY_PUBLISHING_TASK_PRIORITY */

#define SENSOR_PROCESSOR_PRIORITY					(UINT32_C(3)) /**< SENSOR_PROCESSOR_PRIORITY */
#define APP_CONTROLLER_PROCESSOR_PRIORITY			(UINT32_C(3)) /**< APP_CONTROLLER_PROCESSOR_PRIORITY */
#define APP_CMD_CTRL_PROCESSOR_PRIORITY				(UINT32_C(3)) /**< APP_CMD_CTRL_PROCESSOR_PRIORITY */

#define APP_STATUS_PROCESSOR_PRIOIRTY				(UINT32_C(4)) /**< APP_STATUS_PROCESSOR_PRIOIRTY */
#define APP_STATUS_RECURRING_TASK_PRIOIRTY			(UINT32_C(4)) /**< APP_STATUS_RECURRING_TASK_PRIOIRTY */
#define APP_BUTTONS_PROCESSOR_PRIORITY				(UINT32_C(4)) /**< APP_BUTTONS_PROCESSOR_PRIORITY */
#define SERVAL_PROCESSOR_PRIORITY					(UINT32_C(4)) /**< SERVAL_PROCESSOR_PRIORITY */

/**
 * @brief Processor & task parameters.
 */
#define APP_CONTROLLER_PROCESSOR_STACK_SIZE			(UINT32_C(1024)) 	/**< APP_CONTROLLER_PROCESSOR_STACK_SIZE */
#define APP_CONTROLLER_PROCESSOR_QUEUE_LEN			(UINT32_C(10)) 		/**< APP_CONTROLLER_PROCESSOR_QUEUE_LEN */

#define APP_CMD_CTRL_PROCESSOR_STACK_SIZE			(UINT32_C(1024))	/**< APP_CMD_CTRL_PROCESSOR_STACK_SIZE */
#define APP_CMD_CTRL_PROCESSOR_QUEUE_LEN			(UINT32_C(10))		/**< APP_CMD_CTRL_PROCESSOR_QUEUE_LEN */

#define APP_TELEMETRY_SAMPLING_TASK_STACK_SIZE		(UINT32_C(1024))	/**< APP_TELEMETRY_SAMPLING_TASK_STACK_SIZE */

#define APP_TELEMETRY_PUBLISHING_TASK_STACK_SIZE	(UINT32_C(1024))	/**< APP_TELEMETRY_PUBLISHING_TASK_STACK_SIZE */

#define SERVAL_PROCESSOR_STACK_SIZE					(UINT32_C(1600))	/**< SERVAL_PROCESSOR_STACK_SIZE */
#define SERVAL_PROCESSOR_QUEUE_LEN					(UINT32_C(10))		/**< SERVAL_PROCESSOR_QUEUE_LEN */

#define SENSOR_PROCESSOR_STACK_SIZE					(UINT32_C(1024))	/**< SENSOR_PROCESSOR_STACK_SIZE */
#define SENSOR_PROCESSOR_QUEUE_LEN					(UINT32_C(2))		/**< SENSOR_PROCESSOR_QUEUE_LEN */

#define APP_BUTTONS_PROCESSOR_STACK_SIZE			(UINT32_C(1024))	/**< APP_BUTTONS_PROCESSOR_STACK_SIZE */
#define APP_BUTTONS_PROCESSOR_QUEUE_LEN   			(UINT32_C(10))		/**< APP_BUTTONS_PROCESSOR_QUEUE_LEN */

#define APP_STATUS_PROCESSOR_STACK_SIZE				(UINT32_C(1024))	/**< APP_STATUS_PROCESSOR_STACK_SIZE */
#define APP_STATUS_PROCESSOR_QUEUE_LEN   			(UINT32_C(10))		/**< APP_STATUS_PROCESSOR_QUEUE_LEN */
#define APP_STATUS_RECURRING_TASK_STACK_SIZE		(UINT32_C(1024))	/**< APP_STATUS_RECURRING_TASK_STACK_SIZE */
/**@} */


/**
 * @defgroup Solace_App_ModuleID_E Solace_App_ModuleID_E
 * @{
 *
 * @brief BCDS_APP_MODULE_IDs for the Solace App
 *
 * **Usage**
 *
 * At the beginning of very source file:
 *
 * @code
 * 	#include "XdkAppInfo.h"
 * 	#undef BCDS_APP_MODULE_ID
 * 	#define BCDS_APP_MODULE_ID SOLACE_APP_MODULE_ID_xxx
 * @endcode
 *
 */
/**
 * @brief Module Ids for the Solace App.
 */
enum Solace_App_ModuleID_E {
	SOLACE_APP_MODULE_ID_OVERFLOW = XDK_COMMON_ID_OVERFLOW, /**< 62 */
	SOLACE_APP_MODULE_ID_MAIN, 							/**< 63 */
	SOLACE_APP_MODULE_ID_APP_CONTROLLER, 				/**< 64 */
	SOLACE_APP_MODULE_ID_APP_MQTT,						/**< 65 */
	SOLACE_APP_MODULE_ID_APP_XDK_MQTT,					/**< 66 */
	SOLACE_APP_MODULE_ID_APP_CMD_CTRL,					/**< 67 */
	SOLACE_APP_MODULE_ID_APP_CONFIG,					/**< 68 */
	SOLACE_APP_MODULE_ID_APP_MISC,						/**< 69 */
	SOLACE_APP_MODULE_ID_APP_RUNTIME_CONFIG,			/**< 70 */
	SOLACE_APP_MODULE_ID_APP_TELEMETRY_PAYLOAD,			/**< 71 */
	SOLACE_APP_MODULE_ID_APP_TELEMETRY_PUBLISH,			/**< 72 */
	SOLACE_APP_MODULE_ID_APP_TELEMETRY_QUEUE,			/**< 73 */
	SOLACE_APP_MODULE_ID_APP_TELEMETRY_SAMPLING,		/**< 74 */
	SOLACE_APP_MODULE_ID_APP_BUTTONS,					/**< 75 */
	SOLACE_APP_MODULE_ID_APP_STATUS,					/**< 76 */
	SOLACE_APP_MODULE_ID_APP_VERSION,					/**< 77 */
	SOLACE_APP_MODULE_ID_APP_TIMESTAMP,					/**< 78 */
};
/**@} */

/**
 * @defgroup Solace_App_Retcode_E Solace_App_Retcode_E
 * @{
 *
 * @brief Retcodes for the Solace App. Max: 4095.
 *
 */
/**
 * @brief Retcodes for the Solace App.
 */
enum Solace_App_Retcode_E {
    RETCODE_NODE_IPV4_IS_CORRUPTED = RETCODE_XDK_APP_FIRST_CUSTOM_CODE, 	/**< 187 */
	RETCODE_SOLAPP_WLAN_NOT_CONNECTED,										/**< 188 */
	RETCODE_SOLAPP_INVALID_PARAMETER,										/**< 189 */
	RETCODE_SOLAPP_INVALID_CONFIG,											/**< 190 */
	RETCODE_SOLAPP_APP_CONFIG_IS_NULL,										/**< 191 */
	RETCODE_SOLAPP_SD_CARD_NOT_AVAILABLE,									/**< 192 */
	RETCODE_SOLAPP_TELEMETRY_QUEUE_SIZE_IS_ZERO,							/**< 193*/
	RETCODE_SOLAPP_TELEMETRY_QUEUE_CANT_TAKE_SEMAPHORE,						/**< 194 */
	RETCODE_SOLAPP_TELEMETRY_QUEUE_ALREADY_FULL,							/**< 195 */
	RETCODE_SOLAPP_TELEMETRY_QUEUE_NOT_FULL,								/**< 196 */
	RETCODE_SOLAPP_TELEMETRY_PUBLISH_QUEUE_NOT_FULL,						/**< 197 */
	RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT,						/**< 198 */
	RETCODE_SOLAPP_FAILED_TO_SYNC_SNTP_TIME_FROM_SERVER,					/**< 199 */
	RETCODE_SOLAPP_RT_CONFIG_PUBLISH_DATA_LENGTH_EXCEEDS_MAX_LENGTH,		/**< 200 */
	RETCODE_SOLAPP_RT_CONFIG_FAILED_TO_PERSIST,								/**< 201 */
	RETCODE_SOLAPP_NEW_RT_CONFIG_IS_NULL,									/**< 202 */
	RETCODE_SOLAPP_ERROR_READING_SENSOR_VALUES,								/**< 203 */
	RETCODE_SOLAPP_FAILED_TO_CREATE_SEMAPHORE,								/**< 204 */
	RETCODE_SOLAPP_FAILED_TO_TAKE_SEMAPHORE_IN_TIME,						/**< 205 */
	RETCODE_SOLAPP_FAILED_TO_CREATE_TASK,									/**< 206 */
	RETCODE_SOLAPP_CMD_PROCESSOR_IS_NULL,									/**< 207 */
	RETCODE_SOLAPP_FUNCTION_IS_NULL,										/**< 208 */
	RETCODE_SOLAPP_MQTT_PUBLISH_FAILED_NO_CONNECTION,						/**< 209 */
	RETCODE_SOLAPP_MQTT_SUBSCRIBE_FAILED_NO_CONNECTION,						/**< 210 */
	RETCODE_SOLAPP_MQTT_UNSUBSCRIBE_FAILED_NO_CONNECTION,					/**< 211 */
	RETCODE_SOLAPP_MQTT_FAILED_TO_CONNECT_TO_BROKER,						/**< 212 */
	RETCODE_SOLAPP_INVALID_DEFAULT_TELEMETRY_RT_PARAMS,						/**< 213 */
	RETCODE_SOLAPP_APP_RUNTIME_CONFIG_RECEIVED_INVALID_RT_TELEMETRY,		/**< 214 */
	RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ADD_SAMPLE_TO_QUEUE,				/**< 215 */
	RETCODE_SOLAPP_APP_STATUS_FAILED_TO_SEND_QUED_MSGS_NOT_ENABLED,			/**< 216 */
	RETCODE_SOLAPP_MQTT_NOT_CONNECTED,										/**< 217 */
	RETCODE_SOLAPP_UNSUPPORTED_FEATURE,										/**< 218 */
	RETCODE_SOLAPP_APP_CONTROLLER_NOT_READY_TO_APPLY_NEW_RUNTIME_CONFIG,	/**< 219 */
	RETCODE_SOLAPP_INCONSISTENT_TELEMETRY_TASKS,						/**< 220 */
	RETCODE_SOLAPP_CMD_CTRL_FAILED_TO_ENQUEUE_INSTRUCTION_PROCESSING,   /**< 221 */
	RETCODE_SOLAPP_APP_STATUS_ATTEMPT_TO_QUEUE_DUPLICATE_MESSAGE,		/**< 222 */
	RETCODE_SOLAPP_SETUP_AFTER_DISCONNECT,								/**< 223 */
	RETCODE_SOLAPP_APP_CONTROLLER_ENABLE_FAILED,						/**< 224 */
	RETCODE_SOLAPP_APP_CONTROLLER_NOT_READY_TO_EXECUTE_COMMAND, 		/**< 225 */
	RETCODE_SOLAPP_MQTT_PUBLISH_FAILED,									/**< 226 */
	RETCODE_SOLAPP_APP_CONTROLLER_FAILED_TO_ENQUEUE_DISCONNECT_MANAGER,	/**< 227 */
	RETCODE_SOLAPP_APP_STATUS_FAILED_TO_CREATE_RECURRING_TASK,			/**< 228 */
	RETCODE_SOLAPP_MQTT_INVALID_PARAM,									/**< 229 */
	RETCODE_SOLAPP_SD_CARD_FAILED_TO_WRITE_RT_CONFIG,					/**< 230 */
	RETCODE_SOLAPP_SD_CARD_FAILED_TO_RENAME_RT_CONFIG_FILE,				/**< 231 */
	RETCODE_SOLAPP_TEST_CODE,											/**< 232 */
	RETCODE_SOLAPP_SETUP_AFTER_DISCONNECT_MAX_TRIES_REACHED, 			/**< 233 */
	RETCODE_SOLAPP_MQTT_PUBLSIH_PAYLOAD_GT_MAX_PUBLISH_DATA_LENGTH,		/**< 234 */
	RETCODE_SOLAPP_CMD_CTRL_LISTENING_TO_WRONG_TOPICS,					/**< 235 */
	RETCODE_SOLAPP_ERROR_PARSING_BOOTSTRAP_CONFIG,						/**< 236 */
	RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE,					/**< 237 */
	RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE_MAX_Q_LEN_REACHED,	/**< 238 */
	RETCODE_SOLAPP_FAILED_TO_ENQUEUE_STATUS_MESSAGE_DUPLICATE_DETECTED,	/**< 239 */
	RETCODE_SOLAPP_FAILED_TO_SEND_STATUS_MESSAGE,						/**< 240 */
	RETCODE_SOLAPP_FAILED_TO_SEND_TELEMETRY_MESSAGE,					/**< 241 */
	RETCODE_SOLAPP_APP_RT_CONFIG_ACCESS_TO_INTERNAL_PTR_BLOCKED,		/**< 242 */
	RETCODE_SOLAPP_APP_STATUS_FAILED_TO_DELETE_RECURRING_TASK,			/**< 243 */
	RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_PUB_INFO_ERROR,					/**< 244 */
	RETCODE_SOLAPP_APP_XDK_MQTT_UNHANDLED_MQTT_EVENT,					/**< 245 */
	RETCODE_SOLAPP_APP_XDK_MQTT_CALLBACK_FAILED,						/**< 246 */
	RETCODE_SOLAPP_APP_XDK_MQTT_CONNECT_SEMAPHORE_ERROR,				/**< 247 */
	RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_SEMAPHORE_ERROR,				/**< 248 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_SEMAPHORE_ERROR,				/**< 249 */
	RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_SEMAPHORE_ERROR,			/**< 250 */
	RETCODE_SOLAPP_APP_XDK_MQTT_UNSUPPORTED_SCHEME,						/**< 251 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_INIT_FAILED,						/**< 252 */
	RETCODE_SOLAPP_APP_XDK_MQTT_IP_ADDRESS_CONVERSION_FAILED,			/**< 253 */
	RETCODE_SOLAPP_APP_XDK_MQTT_URL_PARSING_FAILED,						/**< 254 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_CONNECT_CALL_FAILED,		/**< 255 */
	RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_CONNECT_CALLBACK_RECEIVED_FROM_BROKER,	/**< 256 */
	RETCODE_SOLAPP_APP_XDK_MQTT_CONNECTION_ERROR_RECEIVED_FROM_BROKER,	/**< 257 */
	RETCODE_SOLAPP_APP_XDK_MQTT_UNKNOWN_CALLBACK_EVENT_RECEIVED,		/**< 258 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_SUBSCRIBE_CALL_FAILED,		/**< 259 */
	RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_SUBSCRIBE_CALLBACK_RECEIVED_FROM_BROKER, /**< 260 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SUBSCRIBE_ERROR_RECEIVED_FROM_BROKER, 	/**< 261 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_UNSUBSCRIBE_CALL_FAILED,  	/**< 262 */
	RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_UNSUBSCRIBE_CALLBACK_RECEIVED_FROM_BROKER,	/**< 263 */
	RETCODE_SOLAPP_APP_XDK_MQTT_UNSUBSCRIBE_ERROR_RECEIVED_FROM_BROKER, /**< 264 */
	RETCODE_SOLAPP_APP_XDK_MQTT_SERVAL_MQTT_PUBLISH_CALL_FAILED, 		/**< 265 */
	RETCODE_SOLAPP_APP_XDK_MQTT_TIMEOUT_NO_PUBLISH_CALLBACK_RECEIVED_FROM_BROKER, 		/**< 266 */
	RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_FAILED, 						/**< 267 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING, 				/**< 268 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING, 				/**< 269 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING, 				/**< 270 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING, 				/**< 271 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_STATE_ERROR, 				/**< 272 */
	RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNDEFINED,					/**< 273 */
	RETCODE_SOLAPP_APP_CONTROLLER_INIT_FAILED,							/**< 274 */
	RETCODE_SOLAPP_APP_CONTROLLER_SETUP_FAILED,							/**< 275 */
	RETCODE_SOLAPP_APP_STATUS_SEMAPHORE_QUEUE_ERROR,					/**< 276 */
	RETCODE_SOLAPP_APP_TELEMETRY_SAMPLING_ERROR_READING_SENSOR_DATA,	/**< 277 */
	RETCODE_SOLAPP_APP_XDK_MQTT_PUBLISH_FAILED_FOR_QOS_1, 				/**< 278 */
	RETCODE_SOLAPP_TELEMETRY_PAYLOAD_UNSUPPORTED_FORMAT, 				/**< 279 */
	RETCODE_SOLAPP_APP_CONTROLLER_BUSY_FAILED_TO_SETUP_AFTER_DISCONNECT, /**< 280 */
	RETCODE_SOLAPP_APPLY_NEW_RUNTIME_CONFIG_TOPIC, 						/**< 281 */
};

/**@} */


/**
 * @defgroup AppStatusMessage_StatusCode_T AppStatusMessage_StatusCode_T
 * @{
 *
 * @brief Status codes for status messages.
 */
/**
 * @brief Enum for the status codes.
 */
typedef enum {
	AppStatusMessage_Status_NULL = -1,
	AppStatusMessage_Status_Info,			/**< 0 */
	AppStatusMessage_Status_Warning,		/**< 1 */
	AppStatusMessage_Status_Error,			/**< 2 */
	AppStatusMessage_Status_Success,		/**< 3 */
	AppStatusMessage_Status_Failed,			/**< 4 */
} AppStatusMessage_StatusCode_T;

/**@} */


 /**
 * @defgroup AppStatusMessage_DescrCode_T AppStatusMessage_DescrCode_T
 * @{
 *
 * @brief Description codes for status messages.
 *
 */
/**
 * @brief Enums for the status description codes.
 */
typedef enum {
	AppStatusMessage_Descr_NULL = 0,																/**< 0 */
	AppStatusMessage_Descr_ErrorParsingJsonBefore, 													/**< 1 */
	AppStatusMessage_Descr_MissingMandatoryElement, 												/**< 2 */
	AppStatusMessage_Descr_ApplyNewMqttBrokerConnectionConfig_NotSupported, 						/**< 3 */
	AppStatusMessage_Descr_AdjustedRtTelemetryConfig_TelemetryRateDown, 							/**< 4 */
	AppStatusMessage_Descr_MqttBrokerWasDisconnected, 												/**< 5 */
	AppStatusMessage_Descr_ErrorParsingRuntimeConfigFileBefore, 									/**< 6 */
	AppStatusMessage_Descr_InvalidPersistedRuntimeConfig_WillDelete, 								/**< 7 */
	AppStatusMessage_Descr_PersistRuntimeConfig2File, 												/**< 8 */
	AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoTopicConfigFound,						/**< 9 */
	AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoMqttBrokerConnectionConfigFound,		/**< 10 */
	AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoStatusConfigFound,						/**< 11 */
	AppStatusMessage_Descr_PersistedRuntimeConfigIsCorrupt_NoTargetTelemetryConfigFound,			/**< 12 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_NotFound,											/**< 13 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_Header,											/**< 14 */
	AppStatusMessage_Descr_CurrentFullStatus, 														/**< 15 */
	AppStatusMessage_Descr_BootStatus, 																/**< 16 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_TopicConfig, 										/**< 17 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_MqttBrokerConnectionConfig, 						/**< 18 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_StatusConfig, 									/**< 19 */
	AppStatusMessage_Descr_PersistedRuntimeConfig_TargetTelemetryConfig, 							/**< 20 */
	AppStatusMessage_Descr_InternalAppError, 														/**< 21 */
	AppStatusMessage_Descr_ActiveTelemetryRuntimeParams, 											/**< 22 */
	AppStatusMessage_Descr_CurrentShortStatus, 														/**< 23 */
	AppStatusMessage_Descr_Test,			 														/**< 24 */
	AppStatusMessage_Descr_DelayIsNegative, 														/**< 25 */
	AppStatusMessage_Descr_DelayGt10, 																/**< 26 */
	AppStatusMessage_Descr_Discarding_StillProcessingPreviousInstruction, 							/**< 27 */
	AppStatusMessage_Descr_UnknownConfigType, 														/**< 28 */
	AppStatusMessage_Descr_MissingMandatoryElementCommand,											/**< 29 */
	AppStatusMessage_Descr_UnknownCommand,															/**< 30 */
	AppStatusMessage_Descr_ExecutingCommand, 														/**< 31 */
	AppStatusMessage_Descr_InvalidTimingsOrFrequency, 												/**< 32 */
	AppStatusMessage_Descr_TelemetryMessageTooLarge, 												/**< 33 */
	AppStatusMessage_Descr_UnknownValue_Apply, 														/**< 34 */
	AppStatusMessage_Descr_MissingMandatoryElement_TopicConfig, 									/**< 35 */
	AppStatusMessage_Descr_TopicConfig_BaseTopicMustHave3Levels, 									/**< 36 */
	AppStatusMessage_Descr_MissingMandatoryElement_MqttBrokerConnectionConfig, 						/**< 37 */
	AppStatusMessage_Descr_MissingMandatoryElement_StatusConfig, 									/**< 38 */
	AppStatusMessage_Descr_StatusConfig_IntervalTooSmall,											/**< 39 */
	AppStatusMessage_Descr_StatusConfig_UnknownValue_PeriodicStatusType,							/**< 40 */
	AppStatusMessage_Descr_StatusConfig_UnknownValue_QoS,											/**< 41 */
	AppStatusMessage_Descr_TelemetryConfig_UnknownValue_SensorsEnable,								/**< 42 */
	AppStatusMessage_Descr_MissingMandatoryElement_TelemetryConfig, 								/**< 43 */
	AppStatusMessage_Descr_MissingMandatoryElement_RuntimeConfig, 									/**< 44 */
	AppStatusMessage_Descr_MqttConfig_IsSecureConnection_True_NotSupported_WillUseFalse, 			/**< 45 */
	AppStatusMessage_Descr_TelemetryConfig_UnknownValue_QoS,										/**< 46 */
	AppStatusMessage_Descr_TelemetryConfig_PaylodFormat_V2_Json_NotSupported_Will_V1_Compatible, 	/**< 47 */
	AppStatusMessage_Descr_TelemetryConfig_UnknownValue_PayloadFormat,								/**< 48 */
	AppStatusMessage_Descr_StatusConfig_QoS_1_Unsupported_Using_QoS_0,								/**< 49 */
	AppStatusMessage_Descr_TelemetryConfig_QoS_1_Unsupported_Using_QoS_0,							/**< 50 */
	AppStatusMessage_Descr_WlanWasDisconnected, 													/**< 51 */

} AppStatusMessage_DescrCode_T;
/**@} */


#endif /* XDK_APPINFO_H_ */

/**@} */
/** ************************************************************************* */
