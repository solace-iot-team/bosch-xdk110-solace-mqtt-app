/*
 * AppStatus.h
 *
 *  Created on: 14 Aug 2019
 *      Author: rjgu
 */
/**
 * @ingroup AppStatus
 * @{
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 **/
#ifndef SOURCE_APPSTATUS_H_
#define SOURCE_APPSTATUS_H_

#include "XdkAppInfo.h"

#include "AppCmdCtrl.h"
#include "AppConfig.h"
#include "AppRuntimeConfig.h"
#include "AppTimestamp.h"

#include "BCDS_Retcode.h"
#include "BCDS_CmdProcessor.h"
#include "cJSON.h"

/**
 * @brief Status message type.
 */
typedef enum {
		AppStatusMessage_Type_NULL = 0,
		AppStatusMessage_Type_Status,
		AppStatusMessage_Type_CmdCtrl
} AppStatusMessage_Type_T;
/**
 * @brief The status message structure.
 */
typedef struct {
	AppStatusMessage_StatusCode_T statusCode; /**< the status code */
	AppStatusMessage_DescrCode_T descrCode; /**< the description code */
	char * details; /**< details string, can be NULL */
	cJSON * items; /**< JSON array of objects, can be NULL */
	char * exchangeId; /**< the optional exchange Id */
	bool isManyParts; /**< flag to indicate this message is part of a sequence */
	uint8_t totalNumParts; /**< total number of parts of the sequence */
	uint8_t thisPartNum; /**< this part number in the sequence */
	AppTimestamp_T createdTimestamp; /**< timestamp when the message was created */
	AppStatusMessage_Type_T type; /**< type of the status message */
	AppCmdCtrlRequestType_T cmdCtrlRequestType; /**< special field for command / control status messages */
	cJSON * tags; /**< optional JSON tags object */
} AppStatusMessage_T;

Retcode_T AppStatus_Init(const char * deviceId, const CmdProcessor_T * processorHandle);

Retcode_T AppStatus_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppStatus_Enable(AppTimestamp_T bootTimestamp);

Retcode_T AppStatus_CreateRecurringTask(void);

Retcode_T AppStatus_SuspendRecurringTask(void);

Retcode_T AppStatus_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * configPtr);

Retcode_T AppStatus_NotifyDisconnectedFromBroker(void);

Retcode_T AppStatus_NotifyReconnected2Broker(void);

AppStatusMessage_T * AppStatus_CreateMessage(const AppStatusMessage_StatusCode_T statusCode, const AppStatusMessage_DescrCode_T descrCode, const char * details);

AppStatusMessage_T * AppStatus_CreateMessageWithExchangeId(const AppStatusMessage_StatusCode_T statusCode, const AppStatusMessage_DescrCode_T descrCode, const char * details, const char * exchangeId);

/* special AppCmdCtrl messages */
AppStatusMessage_T * AppStatus_CmdCtrl_CreateMessage(AppCmdCtrlRequestType_T requestType);

void AppStatus_CmdCtrl_AddExchangeId(AppStatusMessage_T * msg, const char * exchangeId);

void AppStatus_CmdCtrl_AddTags(AppStatusMessage_T * msg, const cJSON * tagsJsonHandle);

void AppStatus_CmdCtrl_AddStatusCode(AppStatusMessage_T * msg, const AppStatusMessage_StatusCode_T statusCode);

void AppStatus_CmdCtrl_AddDescrCode(AppStatusMessage_T * msg, const AppStatusMessage_DescrCode_T descrCode);

void AppStatus_CmdCtrl_AddDetails(AppStatusMessage_T * msg, const char * details);

void AppStatus_CmdCtrl_AddRetcode(AppStatusMessage_T * msg, const Retcode_T retcode);

void AppStatus_AddStatusItem(AppStatusMessage_T * statusMessage, const char * itemName, cJSON * itemJsonHandle);

void AppStatus_SendQueuedMessages(void);

void AppStatus_SendStatusMessage(AppStatusMessage_T * statusMessage);

void AppStatus_SendStatusMessagePart(const AppStatusMessage_DescrCode_T descrCode, const char * details, const char * exchangeId, uint8_t totalNumParts, uint8_t thisPartNum, const char * itemName, cJSON * jsonHandle);

Retcode_T AppStatus_SendBootStatus(void);

void AppStatus_SendCurrentFullStatus(const char * exchangeIdStr);

void AppStatus_SendCurrentShortStatus(const char * exchangeIdStr);

void AppStatus_SendVersionInfo(const char * exchangeIdStr);

void AppStatus_SendActiveTelemetryParams(const char * exchangeIdStr);

void AppStatus_SendMqttBrokerDisconnectedMessage(void);

void AppStatus_SendWlanDisconnectedMessage(void);

void AppStatus_Stats_IncrementMqttBrokerDisconnectCounter(void);

void AppStatus_Stats_IncrementWlanDisconnectCounter(void);

void AppStatus_Stats_IncrementTelemetrySendFailedCounter(void);

void AppStatus_Stats_IncrementTelemetrySendTooSlowCounter(void);

void AppStatus_Stats_IncrementTelemetrySamplingTooSlowCounter(void);

Retcode_T AppStatus_InitErrorHandling(void);

void AppStatus_ErrorHandlingFunc(Retcode_T retcode, bool isfromIsr);

#endif /* SOURCE_APPSTATUS_H_ */

/**@} */
/** ************************************************************************* */



