/*
 * AppTelemetryPublish.h
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTelemetryPublish
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef SOURCE_APPTELEMETRYPUBLISH_H_
#define SOURCE_APPTELEMETRYPUBLISH_H_

#include "AppRuntimeConfig.h"

Retcode_T AppTelemetryPublish_Init(const char * deviceId, uint32_t publishTaskPriority, uint32_t publishTaskStackSize);

Retcode_T AppTelemetryPublish_Setup(const AppRuntimeConfig_T  * configPtr);

Retcode_T AppTelemetryPublish_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr);

Retcode_T AppTelemetryPublish_CreatePublishingTask(void);

Retcode_T AppTelemetryPublish_DeletePublishingTask(void);

bool AppTelemetryPublish_isTaskRunning(void);

#endif /* SOURCE_APPTELEMETRYPUBLISH_H_ */

/**@} */
/** ************************************************************************* */
