
/*
 * AppXDK_MQTT.h
 *
 *  Created on: 31 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppXDK_MQTT
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef SOURCE_APPXDK_MQTT_H_
#define SOURCE_APPXDK_MQTT_H_

#include "BCDS_Retcode.h"
#include "BCDS_CmdProcessor.h"
#include "Serval_Mqtt.h"


#define APP_XDK_MQTT_CONNECT_TIMEOUT_IN_MS                  UINT32_C(60000) /**< connect timeout */
#define APP_XDK_MQTT_SUBSCRIBE_TIMEOUT_IN_MS				UINT32_C(60000) /**< subscribe timeout */
#define APP_XDK_MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS				UINT32_C(60000) /**< unsubscribe timeout */
#define APP_XDK_MQTT_PUBLISH_TIMEOUT_IN_MS					UINT32_C(60000) /**< publish timeout */

/**
 * @brief Enum to represent the supported MQTT types.
 * @note Only serval stack supported at the moment.
 */
typedef enum {
    AppXDK_MQTT_TypeServalStack, /**< serval stack */
} AppXDK_MQTT_Type_T;

/**
 * @brief Structure to represent an incoming MQTT message information.
 */
typedef struct {
    char * topic; /**< The incoming MQTT topic pointer */
    uint32_t topicLength; /**< The incoming MQTT topic length */
    char * payload; /**< The incoming MQTT payload pointer */
    uint32_t payloadLength; /**< The incoming MQTT payload length */
} AppXDK_MQTT_IncomingDataCallbackParam_T;
/**
 * @brief Typedef to the function to be called upon receiving incoming MQTT messages.
 * @param[in] params: the incoming message information
 */
typedef void (*AppXDK_MQTT_IncomingDataCallback_Func_T)(AppXDK_MQTT_IncomingDataCallbackParam_T * params);
/**
 * @brief Callback function typedef for 'connection closed' event.
 */
typedef void (*AppXDK_MQTT_BrokerDisconnectedCallback_Func_T)(void);
/**
 * @brief Structure to represent the MQTT setup features.
 */
typedef struct {
    AppXDK_MQTT_Type_T mqttType; /**< The MQTT type */
    bool isSecure; /**< Boolean representing if we will do a HTTP secure communication.  */
    AppXDK_MQTT_BrokerDisconnectedCallback_Func_T brokerDisconnectCallback_Func; /**< the callback for a 'connection closed' event */
    AppXDK_MQTT_IncomingDataCallback_Func_T incomingDataCallBack_Func; /**< the callback for incoming data */
} AppXDK_MQTT_Setup_T;
/**
 * @brief Structure to represent the MQTT connect features.
 */
typedef struct {
    const char * clientId; /**< The client id to connect with. must be unique across the universe. best to use the device id. */
    const char * brokerUrl; /**< The URL pointing to the MQTT broker */
    uint16_t brokerPort; /**< The port number of the MQTT broker */
    const char * username; /**< username for connecting */
    const char * password; /**< password for connecting */
    bool isCleanSession; /**< The clean session flag indicates to the broker whether the client wants to establish a clean session or a persistent session where all subscriptions and messages (QoS 1 & 2) are stored for the client. */
    uint32_t keepAliveIntervalSecs; /**< The keep alive interval (in seconds) is the time the client commits to for when sending regular pings to the broker. The broker responds to the pings enabling both sides to determine if the other one is still alive and reachable */
} AppXDK_MQTT_Connect_T;
/**
 * @brief Structure to represent the MQTT publish features.
 */
typedef struct {
    char * topic; /**< The MQTT topic to which a message is published on */
    uint32_t qos; /**< The MQTT Quality of Service level. If 0, the message is send in a fire and forget way and it will arrive at most once. If 1 Message reception is acknowledged by the other side, retransmission could occur. */
    const char * payload; /**< Pointer to the payload to be published */
    uint32_t payloadLength; /**< Length of the payload to be published */
} AppXDK_MQTT_Publish_T;
/**
 * @brief Structure to represent the MQTT subscribe features.
 */
typedef struct {
    char * topic; /**< The MQTT topic to subscribe to */
    uint8_t qos; /**< The MQTT Quality of Service level. If 0, the message is send in a fire and forget way and it will arrive at most once. If 1 Message reception is acknowledged by the other side, retransmission could occur. */
} AppXDK_MQTT_Subscribe_T;

Retcode_T AppXDK_MQTT_Init(const AppXDK_MQTT_Setup_T * setupInfoPtr);

Retcode_T AppXDK_MQTT_Setup(void);

Retcode_T AppXDK_MQTT_ConnectToBroker(const AppXDK_MQTT_Connect_T * connectPtr);

Retcode_T AppXDK_MQTT_SubsribeToTopic(const AppXDK_MQTT_Subscribe_T * subscribeInfoPtr);

void AppXDK_MQTT_FreeSubscribeCallbackParams(AppXDK_MQTT_IncomingDataCallbackParam_T * params);

Retcode_T AppXDK_MQTT_UnsubsribeFromTopics(const uint8_t numTopics, const char * topicsStrArray[]);

Retcode_T AppXDK_MQTT_PublishToTopic(const AppXDK_MQTT_Publish_T * publishPtr);


#endif /* SOURCE_APPXDK_MQTT_H_ */

/**@} */
/** ************************************************************************* */
