/*
 * AppMqtt.c
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppMqtt AppMqtt
 * @{
 *
 * @brief Mqtt interface module for the App. Abstracts the underlying implementation.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_MQTT

#include "AppMqtt.h"
#include "AppStatus.h"
#include "AppMisc.h"

#include "BCDS_WlanNetworkConnect.h"

#define APP_MQTT_SUBSCRIBE_WAIT_BEFORE_RETRY_IN_MS		UINT32_C(1000) /**< wait in millis between subscription requests to avoid broker disconnect events while subscribing */


static bool appMqtt_IsConnected2Broker = false; /**< flag to indicate that app is connected to the broker */
static bool appMqtt_IsConnecting2Broker = false; /**< flag to indicate that app is connecting to broker */

static char * appMqtt_DeviceId = NULL; /**< internal device Id */
static AppMqtt_BrokerDisconnectedControllerCallback_Func_T appMqtt_BrokerDisconnectedControllerCallback_Func = NULL; /**< callback for a connection closed event */
static AppXDK_MQTT_IncomingDataCallback_Func_T appMqtt_IncomingDataCallBack_Func = NULL; /**< the global callback for incoming data */

static AppXDK_MQTT_Connect_T appMqtt_MqttConnectInfo; /**< internal connect info config */

/**
 * @brief Called by @ref AppXDK_MQTT module. Simple pass-through to controller module, calling callback set in #AppMqtt_Init().
 * typedef: @ref AppXDK_MQTT_BrokerDisconnectedCallback_Func_T()
 */
static void appMqtt_BrokerDisconnectCallback(void) {
	appMqtt_BrokerDisconnectedControllerCallback_Func();
}
/**
 * @brief Returns if connection to broker is established.
 */
bool AppMqtt_IsConnected(void) {
	return appMqtt_IsConnected2Broker;
}

/**
 * @brief Initialize the module.
 *
 * @param[in] deviceId: the device id
 * @param[in] brokerDisconnectEventCallback: the function to call in case of a connection closed event
 * @param[in] subscriptionIncomingDataCallback: the function to call for any incoming data on any subscription
 *
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppMqtt_Init(const char * deviceId, AppMqtt_BrokerDisconnectedControllerCallback_Func_T brokerDisconnectEventCallback, AppXDK_MQTT_IncomingDataCallback_Func_T subscriptionIncomingDataCallback) {

	assert(deviceId);
	assert(brokerDisconnectEventCallback);
	assert(subscriptionIncomingDataCallback);

	Retcode_T retcode = RETCODE_OK;

	appMqtt_DeviceId = copyString(deviceId);

	appMqtt_BrokerDisconnectedControllerCallback_Func = brokerDisconnectEventCallback;

	appMqtt_IncomingDataCallBack_Func = subscriptionIncomingDataCallback;

	return retcode;
}
/**
 * @brief Setup the module.
 *
 * @param[in] configPtr: the runtime config pointer
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from @ref AppXDK_MQTT_Init()
 * @return Retcode_T: retcode from @ref AppXDK_MQTT_Setup()
 */
Retcode_T AppMqtt_Setup(const AppRuntimeConfig_T * configPtr) {

	assert(configPtr);
	assert(configPtr->mqttBrokerConnectionConfigPtr);

	AppRuntimeConfig_MqttBrokerConnectionConfig_T * mqttConfigPtr = configPtr->mqttBrokerConnectionConfigPtr;
	assert(mqttConfigPtr->received.brokerUrl);
	assert(mqttConfigPtr->received.brokerUsername);
	assert(mqttConfigPtr->received.brokerPassword);
	assert(mqttConfigPtr->received.brokerPort > 0);

	Retcode_T retcode = RETCODE_OK;

	// capture the mqtt setup info
	AppXDK_MQTT_Setup_T appXDK_MqttSetupInfo;
	appXDK_MqttSetupInfo.brokerDisconnectCallback_Func = appMqtt_BrokerDisconnectCallback;
	appXDK_MqttSetupInfo.incomingDataCallBack_Func = appMqtt_IncomingDataCallBack_Func;
	appXDK_MqttSetupInfo.isSecure = configPtr->mqttBrokerConnectionConfigPtr->received.isSecureConnection;
	appXDK_MqttSetupInfo.mqttType = AppXDK_MQTT_TypeServalStack;

	// capture the mqtt connect info
	appMqtt_MqttConnectInfo.brokerUrl = mqttConfigPtr->received.brokerUrl;
	appMqtt_MqttConnectInfo.brokerPort = mqttConfigPtr->received.brokerPort;
	appMqtt_MqttConnectInfo.clientId = appMqtt_DeviceId;
	appMqtt_MqttConnectInfo.isCleanSession = mqttConfigPtr->received.isCleanSession;
	appMqtt_MqttConnectInfo.keepAliveIntervalSecs = mqttConfigPtr->received.keepAliveIntervalSecs;
	appMqtt_MqttConnectInfo.username = mqttConfigPtr->received.brokerUsername;
	appMqtt_MqttConnectInfo.password = mqttConfigPtr->received.brokerPassword;

	if (RETCODE_OK == retcode) retcode = AppXDK_MQTT_Init(&appXDK_MqttSetupInfo);

	if (RETCODE_OK == retcode) retcode = AppXDK_MQTT_Setup();

	return retcode;
}
/**
 * @brief Connect to the broker.
 *
 * @details Checks if the WLAN is connected and calls @ref AppXDK_MQTT_ConnectToBroker().
 *
 * @note Not thread-safe, ensure only one is running at a time.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_WLAN_NOT_CONNECTED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_FAILED_TO_CONNECT_TO_BROKER)
 */
Retcode_T AppMqtt_Connect2Broker(void) {

	Retcode_T retcode = RETCODE_OK;

	#ifdef DEBUG_APP_MQTT
	printf("[INFO] - AppMqtt_Connect2Broker: starting ... \r\n");
	#endif

	// don't do anything if we are currently re-connecting
	if(appMqtt_IsConnecting2Broker) return retcode;

	appMqtt_IsConnecting2Broker = true;
	appMqtt_IsConnected2Broker = false;

	bool wlanConnected = (WLANNWCT_IPSTATUS_CT_AQRD == WlanNetworkConnect_GetIpStatus());

	if(wlanConnected) {

		retcode = AppXDK_MQTT_ConnectToBroker(&appMqtt_MqttConnectInfo);

		appMqtt_IsConnected2Broker = (RETCODE_OK == retcode);

	} else {
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_WLAN_NOT_CONNECTED);
		Retcode_RaiseError(retcode);
	}

	if(appMqtt_IsConnected2Broker) {
		#ifdef DEBUG_APP_MQTT
		printf("[INFO] - AppMqtt_Connect2Broker: MQTT connection successful.\r\n");
		#endif
		retcode = RETCODE_OK;
	} else {
		#ifdef DEBUG_APP_MQTT
		printf("[ERROR] - AppMqtt_Connect2Broker: MQTT connection failed.\r\n");
		#endif
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_FAILED_TO_CONNECT_TO_BROKER);
	}

	appMqtt_IsConnecting2Broker = false;

	return retcode;
}


/**
 * @brief Publish data.
 *
 * @details Checks if app is connected to broker and the payload is not greater than #APP_MQTT_MAX_PUBLISH_DATA_LENGTH.
 * Calls @ref AppXDK_MQTT_PublishToTopic().
 *
 *
 * @param[in] publishInfoPtr: the publish info
 *
 * @return Retcode_T: RETCODE_OK,
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_WARNING, #RETCODE_SOLAPP_MQTT_NOT_CONNECTED)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_PUBLSIH_PAYLOAD_GT_MAX_PUBLISH_DATA_LENGTH)
 * @return Retcode_T: retcode from @ref AppXDK_MQTT_PublishToTopic()
 *
 * @note This function is not thread-safe. Do not modify publishInfoPtr until it is finished.
 *
 * **Example Usage**
 * @code
 *
 * static MQTT_Publish_T mqttPublishInfo = {
 *	.Topic = NULL,
 *	.QoS = 0UL,
 *	.Payload = NULL,
 *	.PayloadLength = 0UL,
 * };
 *
 * cJSON * jsonObject = cJSON_CreateObject();
 *
 * //fill json object with data
 *
 * char * payloadStr = cJSON_PrintUnformatted(jsonObject);
 *
 * mqttPublishInfo.Payload = payloadStr;
 * mqttPublishInfo.PayloadLength = strlen(payloadStr);
 *
 * Retcode_T retcode = AppMqtt_Publish(&mqttPublishInfo);
 *
 * if(RETCODE_OK != retcode) {
 * 		// raise the error from AppMqtt_Publish
 *
 * 		// NOTE: this could send another status message in a different processor
 *		// if severity=Error or Fatal
 *		// hence make sure we are done in this function
 * 		Retcode_RaiseError(retcode);
 *
 * 		// raise your own error in addition if needed
 * 		Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_FAILED_TO_SEND_STATUS_MESSAGE));
 * }
 *
 * cJSON_Delete(jsonObject);
 * free(payloadStr);
 *
 * @endcode
 */
Retcode_T AppMqtt_Publish(const AppXDK_MQTT_Publish_T * publishInfoPtr) {

	assert(publishInfoPtr);

	if(!appMqtt_IsConnected2Broker) return RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_SOLAPP_MQTT_NOT_CONNECTED);

	if(publishInfoPtr->payloadLength > APP_MQTT_MAX_PUBLISH_DATA_LENGTH) {
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_PUBLSIH_PAYLOAD_GT_MAX_PUBLISH_DATA_LENGTH);
	}

	Retcode_T retcode = AppXDK_MQTT_PublishToTopic(publishInfoPtr);

	switch(Retcode_GetCode(retcode)) {
	case RETCODE_OK:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNDEFINED:
		break;
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING:
		appMqtt_IsConnected2Broker = false;
		break;
	default:
		appMqtt_IsConnected2Broker = false;
		break;
	}

	if(RETCODE_OK != retcode) {
		#ifdef DEBUG_APP_MQTT
		printf("[WARNING] - AppMqtt_Publish - MQTT_PublishToTopic() failed \r\n");
		printf("for: %s, qos: %lu \r\n", publishInfoPtr->topic, publishInfoPtr->qos);
		#endif
		// raise an error if qos=1 and error severity
		if(1 == publishInfoPtr->qos && RETCODE_SEVERITY_WARNING != Retcode_GetSeverity(retcode)) {
			Retcode_RaiseError(retcode);
		}
	}

	return retcode;
}

/**
 * @brief Sends a subscription to the broker.
 *
 * @param[in] subscribeInfoPtr: the subscription info
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_NOT_CONNECTED)
 * @return Retcode_T: retcode from @ref AppXDK_MQTT_SubsribeToTopic()
 */
Retcode_T AppMqtt_Subscribe(const AppXDK_MQTT_Subscribe_T * subscribeInfoPtr) {

	assert(subscribeInfoPtr);

	Retcode_T retcode = RETCODE_OK;

	if(!appMqtt_IsConnected2Broker) return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_NOT_CONNECTED);

	retcode = AppXDK_MQTT_SubsribeToTopic(subscribeInfoPtr);

	switch(Retcode_GetCode(retcode)) {
	case RETCODE_OK:
		break;
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING:
		break;
	default:
		appMqtt_IsConnected2Broker = false;
		break;
	}

	if(RETCODE_OK != retcode) {

		#ifdef DEBUG_APP_MQTT
		printf("[WARNING] - AppMqtt_Subscribe - AppXDK_MQTT_SubsribeToTopic() failed \r\n");
		printf("for: %s, qos: %u \r\n", subscribeInfoPtr->topic, subscribeInfoPtr->qos);
		#endif

		Retcode_RaiseError(retcode);

	}

	return retcode;
}
/**
 * @brief Sends multiple unsubscribes in one message to the broker.
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_ERROR, #RETCODE_SOLAPP_MQTT_NOT_CONNECTED)
 * @return Retcode_T: retcode from @ref AppXDK_MQTT_UnsubsribeFromTopics()
 */
Retcode_T AppMqtt_Unsubscribe(const uint8_t numTopics, const char * topicsArray[]) {

	assert(topicsArray);

	Retcode_T retcode = RETCODE_OK;

	if(!appMqtt_IsConnected2Broker) return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SOLAPP_MQTT_NOT_CONNECTED);

	retcode = AppXDK_MQTT_UnsubsribeFromTopics(numTopics, topicsArray);

	switch(Retcode_GetCode(retcode)) {
	case RETCODE_OK:
		break;
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_CONNECTING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_PUBLISHING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_SUBSCRIBING:
	case RETCODE_SOLAPP_APP_XDK_MQTT_MODULE_BUSY_UNSUBSCRIBING:
		break;
	default:
		appMqtt_IsConnected2Broker = false;
		break;
	}

	if(RETCODE_OK != retcode) {

		#ifdef DEBUG_APP_MQTT
		printf("[WARNING] - AppMqtt_Unsubscribe - AppXDK_MQTT_UnsubsribeFromTopics() failed \r\n");
		#endif

		Retcode_RaiseError(retcode);

	}

	return retcode;
}


/**@} */
 /** ************************************************************************* */

