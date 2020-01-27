/*
 * AppTelemetryQueue.h
 *
 *  Created on: 19 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTelemetryQueue
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef SOURCE_APPTELEMETRYQUEUE_H_
#define SOURCE_APPTELEMETRYQUEUE_H_

#include "AppTelemetryPayload.h"
#include "AppRuntimeConfig.h"

Retcode_T AppTelemetryQueue_Init(void);

Retcode_T AppTelemetryQueue_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppTelemetryQueue_Prepare(void);

Retcode_T AppTelemetryQueue_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr);

Retcode_T AppTelemetryQueue_AddSample(AppTelemetryPayload_T * payloadPtr, const uint32_t waitTicks);

Retcode_T AppTelemetryQueue_Wait4FullQueue(const uint32_t waitTicks);

char * AppTelemetryQueue_RetrieveData(void);

/* the test queue */

void AppTelemetryQueueCreateNewTestQueue(uint8_t queueSize);

void AppTelemetryQueueTestQueueAddSample(AppTelemetryPayload_T * payloadPtr);

uint32_t AppTelemetryQueueTestQueueGetDataSize(void);

void AppTelemetryQueueTestQueueDelete(void);

void AppTelemetryQueueTestQueuePrint(void);

#endif /* SOURCE_APPTELEMETRYQUEUE_H_ */

/**@} */
/** ************************************************************************* */
