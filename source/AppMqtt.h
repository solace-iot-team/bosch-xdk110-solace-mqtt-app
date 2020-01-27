/*
 * AppMqtt.h
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppMqtt
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
**/


#ifndef SOURCE_APPMQTT_H_
#define SOURCE_APPMQTT_H_

#include "AppXDK_MQTT.h"
#include "AppRuntimeConfig.h"

/**
 * @brief Max length SERVAL allows for data publishing.
 * @note This is only a 'guess', find out what the max length actually is.
 */
#define APP_MQTT_MAX_PUBLISH_DATA_LENGTH				UINT32_C(900)
//#define APP_MQTT_MAX_PUBLISH_DATA_LENGTH				SERVAL_MAX_SIZE_APP_PACKET // this is not correct

/**
 * @brief Callback function typedef for 'connection closed' event.
 */
typedef void (*AppMqtt_BrokerDisconnectedControllerCallback_Func_T)(void);

bool AppMqtt_IsConnected(void);

Retcode_T AppMqtt_Init(const char * deviceId, AppMqtt_BrokerDisconnectedControllerCallback_Func_T brokerDisconnectEventCallback, AppXDK_MQTT_IncomingDataCallback_Func_T subscriptionIncomingDataCallback);

Retcode_T AppMqtt_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppMqtt_Connect2Broker(void);

Retcode_T AppMqtt_Publish(const AppXDK_MQTT_Publish_T * publishInfoPtr);

Retcode_T AppMqtt_Subscribe(const AppXDK_MQTT_Subscribe_T * subscribeInfoPtr);

Retcode_T AppMqtt_Unsubscribe(const uint8_t numTopics, const char * topicsArray[]);

/**
 * @brief Free the subscription callback parameters.
 * @details Calls @ref AppXDK_MQTT_FreeSubscribeCallbackParams().
 */
static inline void AppMqtt_FreeSubscribeCallbackParams(AppXDK_MQTT_IncomingDataCallbackParam_T * params) {
	AppXDK_MQTT_FreeSubscribeCallbackParams(params);
}

#endif /* SOURCE_APPMQTT_H_ */

/**@} */
/** ************************************************************************* */


