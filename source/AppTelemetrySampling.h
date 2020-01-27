/*
 * AppTelemetrySampling.h
 *
 *  Created on: 25 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTelemetrySampling
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef APPTELEMETRYSAMPLING_H_
#define APPTELEMETRYSAMPLING_H_

#include "AppRuntimeConfig.h"

Retcode_T AppTelemetrySampling_Init(const char * deviceId, uint32_t samplingTaskPriority, uint32_t samplingTaskStackSize, const CmdProcessor_T * sensorProcessorHandle);

Retcode_T AppTelemetrySampling_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppTelemetrySampling_Enable(void);

Retcode_T AppTelemetrySampling_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr);

Retcode_T AppTelemetrySampling_CreateSamplingTask(void);

Retcode_T AppTelemetrySampling_DeleteSamplingTask(void);

bool AppTelemetrySampling_isTaskRunning(void);

#endif /* APPTELEMETRYSAMPLING_H_ */

/**@} */
/** ************************************************************************* */
