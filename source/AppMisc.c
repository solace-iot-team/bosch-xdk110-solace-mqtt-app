/*
 * AppMisc.c
 *
 *  Created on: 22 Jul 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppMisc AppMisc
 * @{
 *
 * @brief Miscellaneous functions.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_MISC

#include "AppMisc.h"
#include "AppVersion.h"
#include "XdkVersion.h"


#include <stdio.h>
#include "XDK_LED.h"

static char * appMisc_DeviceId = NULL; /**< internal device id */

/**
 * @brief Initializes the device id.
 * Call before any other module, most of them use the device id.
 */
void AppMisc_InitDeviceId(void) {

	unsigned int * serialStackAddress0 = (unsigned int*)0xFE081F0;
	unsigned int * serialStackAddress1 = (unsigned int*)0xFE081F4;
	unsigned int serialUnique0 = *serialStackAddress0;
	unsigned int serialUnique1 = *serialStackAddress1;
	size_t sz0;
	sz0 = snprintf(NULL, 0, "%08x%08x", serialUnique1, serialUnique0);

	if(appMisc_DeviceId!=NULL) free(appMisc_DeviceId);

	appMisc_DeviceId = (char *) malloc(sz0 + 1);

	snprintf(appMisc_DeviceId, sz0 + 1, "%08x%08x", serialUnique1, serialUnique0);

	printf("\r\n--------------------------------------------- \r\n");
	printf("XDK Device Id: %s\r\n", appMisc_DeviceId);
	printf("----------------------------------------------- \r\n\r\n");

}
/**
 * @brief Returns the internal device id. Call after initialized.
 */
const char * AppMisc_GetDeviceId(void) {
	assert(appMisc_DeviceId);
	return appMisc_DeviceId;
}
/**
 * @brief Returns the version numbers of the XDK and this App as JSON.
 * @return cJSON *: the version JSON.
 */
cJSON * AppMisc_GetVersionsAsJson(void) {

	cJSON * jsonHandle = cJSON_CreateObject();
	cJSON_AddNumberToObject(jsonHandle, "XDK_Version_Major", XdkVersion_GetMajor());
	cJSON_AddNumberToObject(jsonHandle, "XDK_Version_Minor", XdkVersion_GetMinor());
	cJSON_AddNumberToObject(jsonHandle, "XDK_Version_Patch", XdkVersion_GetPatch());

	cJSON_AddNumberToObject(jsonHandle, "XDK_App_Version", XdkVersion_GetAppVersion());

	cJSON_AddNumberToObject(jsonHandle, "Solace_App_Version_Major", SolaceAppVersion_GetMajor());
	cJSON_AddNumberToObject(jsonHandle, "Solace_App_Version_Minor", SolaceAppVersion_GetMinor());
	cJSON_AddNumberToObject(jsonHandle, "Solace_App_Version_Patch", SolaceAppVersion_GetPatch());

	return jsonHandle;

}
/**
 * @brief Prints the version info on the console.
 */
void AppMisc_PrintVersionInfo(void) {
	printf("[Solace App Version Info]\r\n");
	printJSON(AppMisc_GetVersionsAsJson());
	printf("-------------------------\r\n");
}

/**
 * @brief Create a topic string according to the template.
 * @details The template specifies the format and the parameters the substitutions in the template.
 *
 * @param[in] template: the snprintf template, e.g. "%s/iot-control/%s/device/%s/command"
 * @param[in] method: the first substitution
 * @param[in] baseTopic: the second substitution
 * @param[in] deviceId: the third substitution
 * @return char *: the topic created with malloc()
 *
 * **Example Usage:**
 * @code
 *
 * char * topic = AppMisc_FormatTopic("%s/iot-control/%s/device/%s/command", methodCreate, baseTopic, appCmdCtrl_DeviceId);
 *
 * @endcode
 */
char* AppMisc_FormatTopic(const char* template, const char * method, const char* baseTopic, const char * deviceId) {
	char* destination = NULL;
	size_t sz0;
	sz0 = snprintf(NULL, 0, template, method, baseTopic, deviceId);
	destination = (char*) malloc(sz0 + 1);
	snprintf(destination, sz0 + 1, template, method, baseTopic, deviceId);
	return destination;
}

/**
 * @brief Sets the LEDs into rolling mode to indicate to user that device is in 'set-up'/'connecting' mode.
 * @note Setup and enable LEDs before calling.
 *
 * **Example Usage:**
 * @code
 * 	if (RETCODE_OK == retcode) retcode = LED_Setup();
 * 	if (RETCODE_OK == retcode) retcode = LED_Enable();
 * 	if (RETCODE_OK == retcode) AppMisc_UserFeedback_InSetup();
 * @endcode
 */
void AppMisc_UserFeedback_InSetup(void) {
	LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));
}
/**
 * @brief Switches the orange LED into blinking mode to indicate to user that device is ready/operational.
 * @note Setup and enable LEDs before calling.
 */
void AppMisc_UserFeedback_Ready(void) {
	LED_Pattern(false, LED_PATTERN_ROLLING, 0UL);
	LED_Off(LED_INBUILT_RED);
	LED_Off(LED_INBUILT_YELLOW);
	LED_Blink(true, LED_INBUILT_ORANGE, MILLISECONDS(500), MILLISECONDS(500));
}

/**
 * @brief Creates a copy of a NULL terminated string.
 * @param[in] str : the string to be copied.
 * @return char * : the copied string. NULL if str is NULL.
 */
char * copyString(const char * str) {
	if(NULL == str) return NULL;
	char * copy;
	size_t len = strlen(str) + 1;
	if (!(copy = (char*)malloc(len))) return NULL;
	memset(copy, '\0', len);
	memcpy(copy,str,len-1);
	return copy;
}
/**
 * @brief Counts the number of occurences of c in str
 * @param[in] str: the string
 * @param[in] c: the single character
 * @return size_t: the number of occurrences of c in str
 */
size_t countCharOccurencesInString(char * str, char c) {
	size_t count = 0;
	while(*str) if (*str++ == c) ++count;
	return count;
}

/**
 * @brief Prints the json onto the console.
 * @param[in] jsonHandle: the JSON
 */
void printJSON(cJSON * jsonHandle) {
	if(jsonHandle) {
		char * jsonStr = cJSON_Print(jsonHandle);
		printf("%s\r\n", jsonStr);
		free(jsonStr);
	} else printf("NULL");
}

#ifdef UNUSED
/**
 * @brief Prints the free heap size on console.
 * Used to test for memory leaks for specific processing units.
 *
 * @note not sure this actually works
 *
 */
static void printFreeHeap(void) {
	size_t freeHeap = xPortGetFreeHeapSize();
	//configTOTAL_HEAP_SIZE;
	printf("[TEST] - +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ \r\n");
	printf("[TEST] - printFreeHeap: freeHeap = %i\r\n", freeHeap);
	printf("[TEST] - +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ \r\n");
}
#endif

/**@} */
/** ************************************************************************* */

