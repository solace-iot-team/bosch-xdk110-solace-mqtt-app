/*
 * AppMisc.h
 *
 *  Created on: 22 Jul 2019
 *      Author: rjgu
 */
/**
* @ingroup AppMisc
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
**/

#ifndef SOURCE_APPMISC_H_
#define SOURCE_APPMISC_H_

#include "XdkAppInfo.h"
#include "cJSON.h"
#include "FreeRTOS.h"

/**
 * @brief Calculate ticks from milliseconds
 */
#define MILLISECONDS(x) ((portTickType) x / portTICK_RATE_MS)
/**
 * @brief Calculate ticks from seconds
 */
#define SECONDS(x) ((portTickType) (x * 1000) / portTICK_RATE_MS)

void AppMisc_InitDeviceId(void);

const char * AppMisc_GetDeviceId(void);

cJSON * AppMisc_GetVersionsAsJson(void);

void AppMisc_PrintVersionInfo(void);

char* AppMisc_FormatTopic(const char* template, const char * method, const char* baseTopic, const char * deviceId);

void AppMisc_UserFeedback_InSetup(void);

void AppMisc_UserFeedback_Ready(void);

char * copyString(const char * str);

size_t countCharOccurencesInString(char * str, char c);

void printJSON(cJSON * jsonHandle);

#endif /* SOURCE_APPMISC_H_ */

/**@} */
/** ************************************************************************* */


