/*
 * AppTimestamp.c
 *
 *  Created on: 21 Sep 2019
 *      Author: rjgu
 */

/**
 * @defgroup AppTimestamp AppTimestamp
 * @{
 *
 * @brief Manages timestamps and SNTP interactions.
 *
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_TIMESTAMP

#include "AppTimestamp.h"
#include "AppMisc.h"

#include <stdio.h>
#include <time.h>


static bool appTimestamp_isEnabled = false; /**< flag if module is enable or not */

/**
 * @brief Timestamp string format to milliseconds accuracy. ISO 8601 Date and Time format.
 */
#define APP_TIMESTAMP_STRING_FORMAT				"20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ"
/**
 * @brief Number of times to try retrieve the time from the SNTP server.
 */
#define APP_TIMESTAMP_NUM_SNTP_TRIES			(1000)

static TickType_t appTimestamp_ServerSNTPTimeTickOffset = 0UL; /**< tick count at time of @ref AppTimestamp_Enable() */
static uint64_t appTimestamp_ServerSNTPTimeMillis = 0UL; /**< milliseconds timestamp at time of @ref AppTimestamp_Enable() */

/**
 * @brief Initialize the timestamp module. Does nothing at the moment.
 * @return Retcode_T: RETCODE_OK
 */
Retcode_T AppTimestamp_Init(void) { return RETCODE_OK; }
/**
 * @brief Setup the timestamp module. Calls SNTP_Setup().
 * @param[in] sntpSetupInfoPtr: contains the SNTP setup info.
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: retcode from SNTP_Setup().
 */
Retcode_T AppTimestamp_Setup(SNTP_Setup_T const * const sntpSetupInfoPtr) {

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = SNTP_Setup( (SNTP_Setup_T *) sntpSetupInfoPtr);

	return retcode;
}
/**
 * @brief Enable the module.
 * @details Enables the SNTP module, contacts the SNTP server, retrieves the current time and uses current tick counts as the baseline.
 * Tries #APP_TIMESTAMP_NUM_SNTP_TRIES times before it gives up.
 * Call after #AppTimestamp_Setup().
 *
 * @return Retcode_T: RETCODE_OK
 * @return Retcode_T: RETCODE(RETCODE_SEVERITY_FATAL, #RETCODE_SOLAPP_FAILED_TO_SYNC_SNTP_TIME_FROM_SERVER)
 */
Retcode_T AppTimestamp_Enable(void) {

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = SNTP_Enable();

	if (RETCODE_OK == retcode) {
		int sntpTriesloopCounter = 0;
		bool sntpSuccess = false;

		uint64_t appTimestamp_ServerSNTPTimeSeconds = 0UL;

		while (!sntpSuccess && sntpTriesloopCounter++ < APP_TIMESTAMP_NUM_SNTP_TRIES) {

			printf("[WARNING] - AppTimestamp_Enable: retrieving time from SNTP server, tries: %d\r\n", sntpTriesloopCounter);

			retcode = SNTP_GetTimeFromServer(&appTimestamp_ServerSNTPTimeSeconds, 10000L);
			appTimestamp_ServerSNTPTimeTickOffset = xTaskGetTickCount();
			appTimestamp_ServerSNTPTimeMillis = appTimestamp_ServerSNTPTimeSeconds * 1000;

			if( RETCODE_OK == retcode ) sntpSuccess = true;

			if(!sntpSuccess) {
				printf("[WARNING] - AppTimestamp_Enable: SNTP server timeout, retrying in 1 second ...\r\n");
				vTaskDelay(MILLISECONDS(1000));
			}
		}
		if(!sntpSuccess) {
			retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_SOLAPP_FAILED_TO_SYNC_SNTP_TIME_FROM_SERVER);
		} else {
			appTimestamp_isEnabled = true;
			printf("[INFO] - AppTimestamp_Enable: success.\r\n");
		}
	}
	return retcode;
}
/**
 * @brief Returns the timestamp of tickCount.
 * @details If module is enabled:
 * @details timestamp.secondsSinceEpoch & timestamp.millis are set and timestamp.isTickCount = false
 * @details If module is not enabled:
 * @details timestamp.isTickCount = true and timestamp.tickCount = tickCount
 *
 * @param[in] tickCount: the tick count for which to calculate the timestamp from.
 * @return AppTimestamp_T: the timestamp
 *
 * **Example Usage:**
 * @code
 * 	AppTimestamp_T currentTimestamp = AppTimestamp_GetTimestamp(xTaskGetTickCount());
 * 	if(currentTimestamp.isTickCount) {
 *
 * 		// not enabled yet
 *
 * 	} else {
 *
 * 		// enabled
 * 	}
 * @endcode
 */
AppTimestamp_T AppTimestamp_GetTimestamp(const TickType_t tickCount) {

	AppTimestamp_T timestamp;

	if(appTimestamp_isEnabled) {

		/**
		 * wrong use, to capture past time use AppTimestamp_T, not tickCount
		 */
		assert(tickCount > appTimestamp_ServerSNTPTimeTickOffset);

		TickType_t ticks = tickCount - appTimestamp_ServerSNTPTimeTickOffset;

		uint64_t millisSinceEpoch = (uint64_t) ticks + appTimestamp_ServerSNTPTimeMillis;

		timestamp.secondsSinceEpoch = millisSinceEpoch / 1000;

		timestamp.millis = millisSinceEpoch % 1000;

		timestamp.isTickCount = false;

	} else {
		timestamp.isTickCount = true;
		timestamp.tickCount = tickCount;
	}

	return timestamp;
}
/**
 * @brief Returns the formatted timestamp string for the timestamp.
 * @details If timestamp.isTickCount==true, it calculates the past time.
 *
 * @see #APP_TIMESTAMP_STRING_FORMAT
 *
 * @param[in] timestamp: the timestamp generated with @ref AppTimestamp_GetTimestamp()
 *
 * @return char *: the timestamp converted to string or NULL if module is not enabled yet.
 *
 * @note Returned char * must be deleted after use.
 *
 * **Example Usage:**
 * @code
 *
 *  // lock in the timestamp
 * 	AppTimestamp_T savedTimestamp = AppTimestamp_GetTimestamp(xTaskGetTickCount());
 *
 * 	// do something in between
 *
 * 	char * timestampStr = AppTimestamp_CreateTimestampStr(savedTimestamp);
 *
 * 	// do something
 *
 * 	free(timestampStr);
 *
 * @endcode
 */
char * AppTimestamp_CreateTimestampStr(AppTimestamp_T timestamp) {

	if(!appTimestamp_isEnabled) return NULL;

	if(timestamp.isTickCount) {
		// calculate past time
		assert(appTimestamp_ServerSNTPTimeTickOffset > timestamp.tickCount);

		TickType_t ticks = appTimestamp_ServerSNTPTimeTickOffset - timestamp.tickCount;

		uint64_t millisSinceEpoch = appTimestamp_ServerSNTPTimeMillis - ((uint64_t) ticks);

		timestamp.secondsSinceEpoch = millisSinceEpoch / 1000;

		timestamp.millis = millisSinceEpoch % 1000;

		timestamp.isTickCount = false;

	}

	time_t tt = (time_t) timestamp.secondsSinceEpoch;

	struct tm * gmTime = gmtime(&tt);

	size_t sz;

	sz = snprintf(NULL, 0, APP_TIMESTAMP_STRING_FORMAT,
			gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
			gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, timestamp.millis);

	char * timestampStr = (char *) malloc(sz + 1);

	snprintf(timestampStr, sz + 1, APP_TIMESTAMP_STRING_FORMAT,
			gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
			gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, timestamp.millis);

	return timestampStr;

}



/**@} */
/** ************************************************************************* */


