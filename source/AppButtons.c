/*
 * AppButtons.c
 *
 *  Created on: 14 Aug 2019
 *      Author: rjgu
 */

/**
 * @defgroup AppButtons AppButtons
 * @{
 *
 * @brief Publishes button pressed and released events.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_BUTTONS

#include "AppButtons.h"
#include "AppConfig.h"
#include "AppMisc.h"
#include "AppTimestamp.h"
#include "AppMqtt.h"
#include "BSP_BoardType.h"
#include "BCDS_BSP_Button.h"

typedef enum BSP_Button_E AppButtonNumber_T; /**< enum for the button number */
typedef enum BSP_ButtonPress_E AppButtonEvent_T; /**< enum for the button event */

/**
 * @brief Structure to capture a button event.
 */
typedef struct {
	AppButtonNumber_T buttonNumber; /**< the button number */
	AppButtonEvent_T buttonEvent; /**< the button event*/
	AppTimestamp_T timestamp; /**< timestamp of the event */
} AppButtonEventData_T;

/**
 * @brief Publish info for a button event. @note Qos=0 is used.
 */
static AppXDK_MQTT_Publish_T appButton_MqttPublishInfo = {
	.topic = NULL,
	.qos = 0UL,
	.payload = NULL,
	.payloadLength = 0UL,
};
static char * appButtons_MqttPublishTopicStr = NULL; /**< topic string for the button events */

#define PAYLOAD_VALUE_BUTTON_PRESSED	"PRESSED" /**< json value when button is pressed */

#define PAYLOAD_VALUE_BUTTON_RELEASED	"RELEASED" /**< json value when button is released */

static const CmdProcessor_T * appButtons_ProcessorHandle = NULL; /**< processor handle for the module */

static bool appButtons_isEnabled = false; /**< flag to indicate if module is enabled */

static const char * appButtons_DeviceId = NULL; /**< copy of the device Id */

/** forward declarations */
static void appButtons_Button1CallbackFromIsr(uint32_t data);

static void appButtons_Button2CallbackFromIsr(uint32_t data);

static void appButtons_SetPubTopic(const AppRuntimeConfig_TopicConfig_T * topicConfigPtr);

static void appButtons_PublishEvent(void * buttonEventData, uint32_t param2);

/**
 * @brief Creates new #AppButtonEventData_T on the heap.
 */
AppButtonEventData_T * appButtons_CreateNewButtonEventData(void) {
	 AppButtonEventData_T * buttonEventData = malloc(sizeof(AppButtonEventData_T));
	 return buttonEventData;
}
/**
 * @brief Frees heap memory of buttonEventDataPtr.
 */
void appButtons_DeleteEventData(AppButtonEventData_T * buttonEventDataPtr) {
	free(buttonEventDataPtr);
}
/**
 * @brief Initialize the module.
 * @param[in] deviceId : the device Id
 * @param[in] processorHandle : the processor handle for enqueuing button events
 * @return Retcode_T : RETCODE_OK
 */
Retcode_T AppButtons_Init(const char * deviceId, const CmdProcessor_T * processorHandle) {

	assert(deviceId);
	assert(processorHandle);

	Retcode_T retcode = RETCODE_OK;

	appButtons_DeviceId = copyString(deviceId);

	appButtons_ProcessorHandle = processorHandle;

	return retcode;
}
/**
 * @brief Setup the module. Sets the topic string for publishing button events based on the runtime config.
 * @param[in] configPtr: the runtime config
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppButtons_Setup(const AppRuntimeConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->topicConfigPtr);

	Retcode_T retcode = RETCODE_OK;

	appButtons_SetPubTopic(configPtr->topicConfigPtr);

	return retcode;
}
/**
 * @brief Enable the module. Enables the BSP_Button module and registers the callbacks for button 1 & 2.
 * @return Retcode_T: RETCODE_OK, retcode from called functions.
 */
Retcode_T AppButtons_Enable(void) {

    Retcode_T retcode = RETCODE_OK;

    retcode = BSP_Button_Connect();

    if(RETCODE_OK == retcode) retcode = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_1, appButtons_Button1CallbackFromIsr);

    if(RETCODE_OK == retcode) retcode = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_2, appButtons_Button2CallbackFromIsr);

    appButtons_isEnabled = true;

    return retcode;
}
/**
 * @brief Applies the new runtime configuration for topics.
 * @param[in] configElement: the configuration element the configPtr contains.
 * @param[in] configPtr: the new configuration
 * @return Retcode_T: RETCODE_OK or RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT)
 * @note supports only AppRuntimeConfig_Element_topicConfig
 */
Retcode_T AppButtons_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * const configPtr) {

	assert(configPtr);

	Retcode_T retcode = RETCODE_OK;

	if(AppRuntimeConfig_Element_topicConfig == configElement) {

		appButtons_SetPubTopic((AppRuntimeConfig_TopicConfig_T *) configPtr);

	} else {

		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_UNSUPPORTED_RUNTIME_CONFIG_ELEMENT);
	}

	return retcode;
}
/**
 * @brief Notification that broker is disconnected. Sets #appButtons_isEnabled to false.
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppButtons_NotifyDisconnectedFromBroker(void) {

	appButtons_isEnabled = false;

	return RETCODE_OK;
}
/**
 * @brief Notification that broker is reconnected. Sets #appButtons_isEnabled to true;
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppButtons_NotifyReconnected2Broker(void) {

	appButtons_isEnabled = true;

	return RETCODE_OK;
}
/**
 * @brief Constructs the topic from the runtime configuration and stores it.
 * @param[in] topicConfigPtr: the runtime topic configuration.
 */
static void appButtons_SetPubTopic(const AppRuntimeConfig_TopicConfig_T * const topicConfigPtr) {

	assert(topicConfigPtr);

	if(appButtons_MqttPublishTopicStr) free(appButtons_MqttPublishTopicStr);

	appButtons_MqttPublishTopicStr = AppMisc_FormatTopic("%s/iot-event/%s/device/%s/button", topicConfigPtr->received.methodCreate, topicConfigPtr->received.baseTopic, appButtons_DeviceId);

	appButton_MqttPublishInfo.topic = appButtons_MqttPublishTopicStr;
}
/**
 * @brief Callback for BSP button module for button 1 event. Enqueues #appButtons_PublishEvent() for processing.
 * @param[in] data: BSP_ButtonPress_T, the button event
 * @exception Retcode_RaiseErrorFromIsr: if enqueuing fails
 */
static void appButtons_Button1CallbackFromIsr(uint32_t data) {

	Retcode_T retcode = RETCODE_OK;

    AppButtonEventData_T * buttonEventData = appButtons_CreateNewButtonEventData();
	buttonEventData->buttonNumber = BSP_XDK_BUTTON_1;
	buttonEventData->buttonEvent = (BSP_ButtonPress_T) data;
	buttonEventData->timestamp = AppTimestamp_GetTimestamp(xTaskGetTickCount());

	retcode = CmdProcessor_EnqueueFromIsr( (CmdProcessor_T *) appButtons_ProcessorHandle, appButtons_PublishEvent, buttonEventData, UINT32_C(0));
	if(RETCODE_OK != retcode) {
		Retcode_RaiseErrorFromIsr(retcode);
	}
}
/**
 * @brief Callback for BSP button module for button 2 event. Enqueues #appButtons_PublishEvent() for processing.
 * @param[in] data: BSP_ButtonPress_T, the button event
 * @exception Retcode_RaiseErrorFromIsr: if enqueuing fails
 */
static void appButtons_Button2CallbackFromIsr(uint32_t data) {

	Retcode_T retcode = RETCODE_OK;

    AppButtonEventData_T * buttonEventData = appButtons_CreateNewButtonEventData();
	buttonEventData->buttonNumber = BSP_XDK_BUTTON_2;
	buttonEventData->buttonEvent = (BSP_ButtonPress_T) data;
	buttonEventData->timestamp = AppTimestamp_GetTimestamp(xTaskGetTickCount());

	retcode = CmdProcessor_EnqueueFromIsr( (CmdProcessor_T *) appButtons_ProcessorHandle, appButtons_PublishEvent, buttonEventData, UINT32_C(0));
	if(RETCODE_OK != retcode) {
		Retcode_RaiseErrorFromIsr(retcode);
	}
}

/**
 * @brief Publish the button event data as JSON.
 * Enqueue from button callback function to be executed by the command processor.
 *
 * @param[in] buttonEventData: #AppButtonEventData_T
 * @param[in] param2: unused
 *
 * @exception Retcode_RaiseError: result of @ref AppMqtt_Publish() if not RETCODE_OK
 */
static void appButtons_PublishEvent(void * buttonEventData, uint32_t param2) {

    BCDS_UNUSED(param2);

	#ifdef DEBUG_APP_BUTTONS
    printf("[INFO] - appButtons_PublishEvent: button event received. appButtons_isEnabled=%u\r\n", appButtons_isEnabled);
	#endif

    // simply ignore. no queueing.
    if(!appButtons_isEnabled) return;

	AppButtonEventData_T * buttonEventDataPtr = (AppButtonEventData_T *) buttonEventData;

	cJSON *payloadJsonHandle = cJSON_CreateObject();

	cJSON_AddItemToObject(payloadJsonHandle, "timestamp", cJSON_CreateString(AppTimestamp_CreateTimestampStr(buttonEventDataPtr->timestamp)));

	cJSON_AddItemToObject(payloadJsonHandle, "deviceId", cJSON_CreateString(appButtons_DeviceId));

	cJSON_AddNumberToObject(payloadJsonHandle, "buttonNumber", buttonEventDataPtr->buttonNumber);

	switch (buttonEventDataPtr->buttonEvent) {
		case BSP_XDK_BUTTON_PRESS:
			cJSON_AddItemToObject(payloadJsonHandle, "event", cJSON_CreateString(PAYLOAD_VALUE_BUTTON_PRESSED));
		break;
		case BSP_XDK_BUTTON_RELEASE:
			cJSON_AddItemToObject(payloadJsonHandle, "event", cJSON_CreateString(PAYLOAD_VALUE_BUTTON_RELEASED));
		break;
		default:
			assert(0);
	}

	char * payloadStr = cJSON_PrintUnformatted(payloadJsonHandle);

	#ifdef DEBUG_APP_BUTTONS
	printf("[INFO] - AppButtons.publishButtonEvent: topic=%s\r\n", appButton_MqttPublishInfo.Topic);
	printf("[INFO] - AppButtons.publishButtonEvent: payload=\r\n%s\r\n", payloadStr);
	#endif


	appButton_MqttPublishInfo.payload = payloadStr;
	appButton_MqttPublishInfo.payloadLength = strlen(payloadStr);

	Retcode_T retcode = AppMqtt_Publish(&appButton_MqttPublishInfo);

	appButtons_DeleteEventData(buttonEventDataPtr);
	cJSON_Delete(payloadJsonHandle);
	free(payloadStr);

	if(RETCODE_OK != retcode) Retcode_RaiseError(retcode);
}

/**@} */
/** ************************************************************************* */




