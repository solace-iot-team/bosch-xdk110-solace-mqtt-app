/*
 * AppVersion.c
 *
 *  Created on: 11 Sep 2019
 *      Author: rjgu
 */
/**
 * @defgroup AppVersion AppVersion
 * @{
 *
 * @brief This module defines and gives access to the current version of the Solace MQTT app.
 *
 * @author $(SOLACE_APP_AUTHOR)
 *
 * @date $(SOLACE_APP_DATE)
 *
 * @file
 *
 */

#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID /**< undefine any previous module id */
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_APP_VERSION

#include "AppVersion.h"

/**
* @brief  Get the major version of the Solace App.
* @return uint8_t: the major version
*/
uint8_t SolaceAppVersion_GetMajor(void) {
    return SOLACE_APP_VERSION_MAJOR;
}
/**
* @brief  Get the minor version of the Solace App.
* @return uint8_t: the minor version
*/
uint8_t SolaceAppVersion_GetMinor(void) {
    return SOLACE_APP_VERSION_MINOR;
}
/**
* @brief  Get the patch version of the Solace App.
* @return uint8_t: the patch version
*/
uint8_t SolaceAppVersion_GetPatch(void) {
    return SOLACE_APP_VERSION_PATCH;
}

/**@}*/
/** ************************************************************************* */

