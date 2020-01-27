/*
 * AppTelemetryPayload.h
 *
 *  Created on: 26 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTelemetryPayload
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef SOURCE_APPTELEMETRYPAYLOAD_H_
#define SOURCE_APPTELEMETRYPAYLOAD_H_

#include "AppRuntimeConfig.h"
#include "AppTimestamp.h"

#include "XDK_Sensor.h"

/**
 * @brief Payload typedef.
 */
typedef cJSON AppTelemetryPayload_T;

Retcode_T AppTelemetryPayload_Init(const char * deviceId);

Retcode_T AppTelemetryPayload_Setup(const AppRuntimeConfig_T * configPtr);

Retcode_T AppTelemetryPayload_ApplyNewRuntimeConfig(AppRuntimeConfig_ConfigElement_T configElement, const void * newConfigPtr);

AppTelemetryPayload_T * AppTelemetryPayload_CreateNew(const TickType_t tickCount, const Sensor_Value_T * sensorValuePtr);

AppTelemetryPayload_T * AppTelemetryPayload_CreateNew_Test(
		const TickType_t tickCount,
		const Sensor_Value_T * sensorValuePtr,
		const AppRuntimeConfig_Sensors_T * sensorsConfigPtr,
		AppRuntimeConfig_Telemetry_PayloadFormat_T payloadFormat);

void AppTelemetryPayload_Delete(AppTelemetryPayload_T * payloadPtr);

#endif /* SOURCE_APPTELEMETRYPAYLOAD_H_ */

/**@} */
/** ************************************************************************* */
