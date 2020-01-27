/*
 * AppButtons.h
 *
 *  Created on: 14 Aug 2019
 *      Author: rjgu
 */
/**
 * @ingroup AppButtons
 * @{
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 *
 **/

#ifndef SOURCE_APPBUTTONS_H_
#define SOURCE_APPBUTTONS_H_

#include "AppRuntimeConfig.h"

#include "BCDS_Retcode.h"
#include "BCDS_CmdProcessor.h"

Retcode_T AppButtons_Init(const char * deviceId, const CmdProcessor_T * processorHandle);

Retcode_T AppButtons_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppButtons_Enable(void);

Retcode_T AppButtons_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * const configPtr);

Retcode_T AppButtons_NotifyDisconnectedFromBroker(void);

Retcode_T AppButtons_NotifyReconnected2Broker(void);

#endif /* SOURCE_APPBUTTONS_H_ */

/**@} */
/** ************************************************************************* */

