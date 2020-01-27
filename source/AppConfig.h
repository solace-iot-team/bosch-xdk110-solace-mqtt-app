/*
 * AppConfig.h
 *
 *  Created on: 18 Jul 2019
 *      Author: rjgu
 */

/**
* @ingroup AppConfig
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
**/


#ifndef SOURCE_APPCONFIG_H_
#define SOURCE_APPCONFIG_H_

#include "AppMqtt.h"

#include "XDK_WLAN.h"
#include "XDK_SNTP.h"

Retcode_T AppConfig_Init(const char * deviceId);

const SNTP_Setup_T * AppConfig_GetSntpSetupInfoPtr(void);

const WLAN_Setup_T * AppConfig_GetWlanSetupInfoPtr(void);

const AppXDK_MQTT_Connect_T * AppConfig_GetMqttConnectInfoPtr(void);

const char * AppConfig_GetBaseTopicStr(void);

bool AppConfig_GetIsMqttBrokerConnectionSecure(void);


#endif /* SOURCE_APPCONFIG_H_ */

/**@} */
/** ************************************************************************* */
