/*
 * AppTimestamp.h
 *
 *  Created on: 21 Sep 2019
 *      Author: rjgu
 */
/**
* @ingroup AppTimestamp
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
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
	uint64_t secondsSinceEpoch; /**< seconds since the epoch */
	uint16_t millis; /**< milliseconds to add to seconds since epoch */
	TickType_t tickCount; /**< the tick count if #isTickCount is true */
	bool isTickCount; /**< flag to indicate if structure contains tickCount or secondsSinceEpoch & millis */
} AppTimestamp_T;

Retcode_T AppTimestamp_Init(void);

Retcode_T AppTimestamp_Setup(SNTP_Setup_T const * const sntpSetupInfoPtr);

Retcode_T AppTimestamp_Enable(void);

AppTimestamp_T AppTimestamp_GetTimestamp(const TickType_t tickCount);

char * AppTimestamp_CreateTimestampStr(AppTimestamp_T appTimestamp);

#endif /* SOURCE_APPTIMESTAMP_H_ */

/**@} */
/** ************************************************************************* */
