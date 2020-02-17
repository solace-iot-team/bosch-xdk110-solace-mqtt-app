/*
 * AppCmdCtrl.h
 *
 *  Created on: 23 Jul 2019
 *      Author: rjgu
 */
/**
 * @ingroup AppCmdCtrl
 * @{
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 *
 **/
#ifndef SOURCE_APPCMDCTRL_H_
#define SOURCE_APPCMDCTRL_H_

#include "AppRuntimeConfig.h"
#include "AppMqtt.h"

/**
 * @brief The type of instruction.
 */
typedef enum {
	AppCmdCtrl_RequestType_NULL = 0,
    AppCmdCtrl_RequestType_Configuration,
	AppCmdCtrl_RequestType_Command
} AppCmdCtrlRequestType_T;
/**
 * @brief The command instruction.
 */
typedef enum {
	AppCmdCtrl_CommandType_SuspendTelemetry = 0,
	AppCmdCtrl_CommandType_ResumeTelemetry,
	AppCmdCtrl_CommandType_SendFullStatus,
	AppCmdCtrl_CommandType_SendShortStatus,
	AppCmdCtrl_CommandType_SendActiveTelemetryParams,
	AppCmdCtrl_CommandType_SendActiveRuntimeConfig,
	AppCmdCtrl_CommandType_SendRuntimeConfigFile,
	AppCmdCtrl_CommandType_DeleteRuntimeConfigFile,
	AppCmdCtrl_CommandType_PersistActiveConfig,
	AppCmdCtrl_CommandType_Reboot,
	AppCmdCtrl_CommandType_TriggerSampleError,
	AppCmdCtrl_CommandType_TriggerSampleFatalError,
	AppCmdCtrl_CommandType_SendVersionInfo
} AppCmdCtrl_CommandType_T;
/**
 * @brief Callback function for new configuration processing.
 */
typedef Retcode_T (*AppControllerApplyRuntimeConfiguration_Func_T)(AppRuntimeConfig_ConfigElement_T, void *);
/**
 * @brief Callback function for command processing.
 */
typedef Retcode_T (*AppControllerExecuteCommand_Func_T)(AppCmdCtrl_CommandType_T, const char *);


AppXDK_MQTT_IncomingDataCallback_Func_T AppCmdCtrl_GetGlobalSubscriptionCallback(void);

Retcode_T AppCmdCtrl_Init(	const char * deviceId,
							bool isCleanSession,
							const CmdProcessor_T * processorHandle,
							AppControllerApplyRuntimeConfiguration_Func_T appControllerApplyRuntimeConfigurationFunc,
							AppControllerExecuteCommand_Func_T appControllerExecuteCommandFunc);

Retcode_T AppCmdCtrl_Setup(void);

Retcode_T AppCmdCtrl_Enable(const AppRuntimeConfig_T * configPtr);

Retcode_T AppCmdCtrl_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, void * configPtr);

Retcode_T AppCmdCtrl_NotifyReconnected2Broker(void);

Retcode_T AppCmdCtrl_NotifyDisconnectedFromBroker(void);


#endif /* SOURCE_APPCMDCTRL_H_ */

/**@} */
/** ************************************************************************* */
