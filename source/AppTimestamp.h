/*
 * AppTimestamp.h
 *
 *  Created on: 21 Sep 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTimestamp
* @{
*/
#ifndef SOURCE_APPTIMESTAMP_H_
#define SOURCE_APPTIMESTAMP_H_

#include "XDK_SNTP.h"

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Timestamp structure.
 * @details if isTickCount==true, tickCount is set correctly
 * @details if isTickCount==false, secondsSinceEpoch & millis are set correctly
 */
typedef struct {
	uint64_t secondsSinceEpoch;
	uint16_t millis;
	TickType_t tickCount;
	bool isTickCount;
} AppTimestamp_T;

Retcode_T AppTimestamp_Init(void);

Retcode_T AppTimestamp_Setup(SNTP_Setup_T const * const sntpSetupInfoPtr);

Retcode_T AppTimestamp_Enable(void);

AppTimestamp_T AppTimestamp_GetTimestamp(const TickType_t tickCount);

char * AppTimestamp_CreateTimestampStr(AppTimestamp_T appTimestamp);

#endif /* SOURCE_APPTIMESTAMP_H_ */

/**@} */
/** ************************************************************************* */
