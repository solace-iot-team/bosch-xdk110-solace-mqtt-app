/*
 * AppConfig.c
 *
 *  Created on: 18 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppConfig AppConfig
 * @{
 *
 * @brief Reads the bootstrap config json (#APP_CONFIG_FILENAME)
 *
 * @warning Does not check the integrity of the config file and provides little feedback in case of an error.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_CONFIG

#include "AppConfig.h"
#include "AppMisc.h"
#include "AppStatus.h"

#include "XDK_Storage.h"
#include "XDK_Utils.h"

#define APP_CONFIG_DEFAULT_MQTT_KEEP_ALIVE_INTERVAL_IN_SECS			UINT32_C(60) /**< default, the time the broker will keep the connection alive without incoming traffic */
#define APP_CONFIG_DEFAULT_MQTT_CLEAN_SESSION						(false)		/**< the cleanSession flag on the connection */
#define APP_CONFIG_DEFAULT_MQTT_IS_SECURE_CONNECTION				(false)		/**< default, flag if connection should be secure */
#define APP_CONFIG_FILENAME 		"/config.json" /**< The bootstrap config file name on the SD card.*/

/**
 * @brief Structure to hold the bootstrap config values.
 */
typedef struct {
    const char* baseTopic; /**< the 3 level base topic string */
    struct  {
		AppXDK_MQTT_Connect_T mqttConnectInfo; /**< the connect info */
        bool isSecureConnection; /**< flag if connection is secure */
    } mqttBrokerInfo; /**< the broker info */
    SNTP_Setup_T sntpSetupInfo; /**< the SNTP info */
    WLAN_Setup_T wlanSetupInfo; /**< the WLAN info */
} AppConfig_T;

/**
 * @brief Instance of the bootstrap config as read from the file #APP_CONFIG_FILENAME.
 */
static AppConfig_T appConfig_Info = {
	.sntpSetupInfo = {
			.ServerUrl = NULL,
			.ServerPort = 0
	},
	.wlanSetupInfo = {
			.SSID = NULL,
			.Username = NULL,
			.Password = NULL,
			.IsEnterprise = false,
			.IsHostPgmEnabled = false,
			.IsStatic = false,

			.IpAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
			.GwAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
			.DnsAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
			.Mask = XDK_NETWORK_IPV4(0, 0, 0, 0),

	},
	.mqttBrokerInfo = {
			.isSecureConnection = APP_CONFIG_DEFAULT_MQTT_IS_SECURE_CONNECTION,
			.mqttConnectInfo = {
					.clientId = NULL,
					.brokerUrl = NULL,
					.brokerPort = UINT16_C(0),
					.isCleanSession = APP_CONFIG_DEFAULT_MQTT_CLEAN_SESSION,
					.keepAliveIntervalSecs = APP_CONFIG_DEFAULT_MQTT_KEEP_ALIVE_INTERVAL_IN_SECS,
					.username = NULL,
					.password = NULL,
			},
	},
	.baseTopic = NULL
};

static bool appConfig_isSetup = false; /**< internal flag to indicate if module is set up fully */

/**
 * @brief Storage setup structure.
 */
static Storage_Setup_T appConfig_StorageSetup = {
	.SDCard = true, .WiFiFileSystem = false,
};
#define CFG_FILE_READ_BUFFER_SIZE		UINT32_C(1024) 	/**< buffer size to read the bootstrap config file into memory */
static uint8_t appConfig_FileReadBuffer[CFG_FILE_READ_BUFFER_SIZE] = { 0 }; /**< buffer to read config file into memory */

/**
 * @brief Storage read credentials config.
 */
static Storage_Read_T appConfig_ReadCredentials = {
	.FileName = APP_CONFIG_FILENAME,
	.ReadBuffer = appConfig_FileReadBuffer,
	.BytesToRead = sizeof(appConfig_FileReadBuffer),
	.ActualBytesRead = 0UL,
	.Offset = 0UL
};

static cJSON * appConfig_ReadConfigFromFile(void);

/**
 * @brief Returns the SNTP setup info as read from the config.
 * @return SNTP_Setup_T *: the setup info
 */
const SNTP_Setup_T * AppConfig_GetSntpSetupInfoPtr(void) {
	assert(appConfig_isSetup);
	return &(appConfig_Info.sntpSetupInfo);
}
/**
 * @brief Returns the WLAN setup info as read from the config.
 * @return WLAN_Setup_T *: the setup info.
 */
const WLAN_Setup_T * AppConfig_GetWlanSetupInfoPtr(void) {
	assert(appConfig_isSetup);
	return &(appConfig_Info.wlanSetupInfo);
}
/**
 * @brief Returns the Mqtt connect info.
 * @return AppXDK_MQTT_Connect_T *: the connect info.
 */
const AppXDK_MQTT_Connect_T * AppConfig_GetMqttConnectInfoPtr(void) {
	assert(appConfig_isSetup);
	return &(appConfig_Info.mqttBrokerInfo.mqttConnectInfo);
}
/**
 * @brief Returns the isSecure flag.
 * @return bool : the flag.
 */
bool AppConfig_GetIsMqttBrokerConnectionSecure(void) {
	assert(appConfig_isSetup);
	return appConfig_Info.mqttBrokerInfo.isSecureConnection;
}
/**
 * @brief Returns baseTopic string.
 * @return char * : the baseTopic string.
 */
const char * AppConfig_GetBaseTopicStr(void) {
	assert(appConfig_isSetup);
	return appConfig_Info.baseTopic;
}
/**
 * @brief Initialize the module.
 * @details Reads the main or bootstrap config file, #APP_CONFIG_FILENAME and populates internal structure #appConfig_Info.
 *
 * @warning Does not perform any checking for correctness of the config file.
 *
 * @note 'brokerSecureConnection' flag is not supported and always set to false
 *
 * @param[in] deviceId : the device Id. Used as the clientId for the MQTT session.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: return from Storage_Setup(), Storage_Enable(), Storage_IsAvailable(),
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_SD_CARD_NOT_AVAILABLE)
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_ERROR_PARSING_BOOTSTRAP_CONFIG)
 */
Retcode_T AppConfig_Init(const char * deviceId) {

	assert(deviceId);

	Retcode_T retcode = RETCODE_OK;

    if (RETCODE_OK == retcode) retcode = Storage_Setup(&appConfig_StorageSetup);

    if (RETCODE_OK == retcode) retcode = Storage_Enable();

    bool status = false;
    if (RETCODE_OK == retcode) retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &status);

    cJSON * configJSON = NULL;
    if ((RETCODE_OK == retcode) && (status == true)){

    	configJSON = appConfig_ReadConfigFromFile();

    } else {
		return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_SD_CARD_NOT_AVAILABLE);
    }

    if(configJSON == NULL) {
    	return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_ERROR_PARSING_BOOTSTRAP_CONFIG);
    }
    // copy config into internal structure first
    // - not checking if each item acutally exists

    appConfig_Info.wlanSetupInfo.SSID = copyString(cJSON_GetObjectItem(configJSON,"wlanSSID")->valuestring);
    appConfig_Info.wlanSetupInfo.Username = copyString(cJSON_GetObjectItem(configJSON,"wlanPSK")->valuestring);
	appConfig_Info.wlanSetupInfo.Password = copyString(cJSON_GetObjectItem(configJSON,"wlanPSK")->valuestring);

    appConfig_Info.sntpSetupInfo.ServerUrl = copyString(cJSON_GetObjectItem(configJSON,"sntpURL")->valuestring);
    appConfig_Info.sntpSetupInfo.ServerPort = cJSON_GetObjectItem(configJSON,"sntpPort")->valueint;

    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.brokerUrl = copyString(cJSON_GetObjectItem(configJSON,"brokerUrl")->valuestring);
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.brokerPort = cJSON_GetObjectItem(configJSON,"brokerPort")->valueint;
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.username = copyString(cJSON_GetObjectItem(configJSON,"brokerUsername")->valuestring);
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.password = copyString(cJSON_GetObjectItem(configJSON,"brokerPassword")->valuestring);
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.clientId = copyString(deviceId);
    // always set clean session to false at boot
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.isCleanSession = APP_CONFIG_DEFAULT_MQTT_CLEAN_SESSION;
    appConfig_Info.mqttBrokerInfo.mqttConnectInfo.keepAliveIntervalSecs = cJSON_GetObjectItem(configJSON, "brokerKeepAliveIntervalSecs")->valueint;

    appConfig_Info.mqttBrokerInfo.isSecureConnection = false;
#ifdef SECURE_CONNECTION_IS_SUPPORTED
    bool isSecureConnection = cJSON_GetObjectItem(configJSON, "brokerSecureConnection")->valueint;
    if(isSecureConnection) {
    	// not supported at the moment
    	AppStatusMessage_T * msg = AppStatus_CreateMessage(AppStatusMessage_Status_Warning, AppStatusMessage_Descr_MqttConfig_IsSecureConnection_True_NotSupported_WillUseFalse, "brokerSecureConnection");
    	AppStatus_SendStatusMessage(msg);
    }
#endif

    appConfig_Info.baseTopic = copyString(cJSON_GetObjectItem(configJSON,"baseTopic")->valuestring);

    cJSON_Delete(configJSON);

	appConfig_isSetup = true;

	return retcode;
}
/**
 * @brief Reads the config file and parses the JSON.
 *
 * @return cJSON *: the parsed JSON or NULL in case of an error
 */
static cJSON * appConfig_ReadConfigFromFile(void) {

	Retcode_T retcode = RETCODE_OK;

	retcode = Storage_Read(STORAGE_MEDIUM_SD_CARD, &appConfig_ReadCredentials);

	if (retcode!= RETCODE_OK) {
    	Retcode_RaiseError(retcode);
    	return NULL;
    }
    cJSON * configJSON = cJSON_ParseWithOpts((const char *) appConfig_FileReadBuffer, 0, 1);
	if (!configJSON) {
		vTaskDelay(2000);
		printf("[ERROR] - appConfig_ReadConfigFromFile : parsing config file, before: [%s]\r\n", cJSON_GetErrorPtr());
		return NULL;
	}

	printf("[INFO] - appConfig_ReadConfigFromFile: the config.json:\r\n");
	printJSON(configJSON);

	return configJSON;
}

/**@} */
/** ************************************************************************* */

