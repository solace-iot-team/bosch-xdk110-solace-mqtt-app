/*
 * AppVersion.h
 *
 *  Created on: 11 Sep 2019
 *      Author: rjgu
 */
/**
* @ingroup AppVersion
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

#ifndef SOURCE_APPVERSION_H_
#define SOURCE_APPVERSION_H_

#include <stdint.h>

#define SOLACE_APP_VERSION_MAJOR		2 /**< the major version */

#define SOLACE_APP_VERSION_MINOR		0 /**< the minor version */

#define SOLACE_APP_VERSION_PATCH		1 /**< the patch version */


uint8_t SolaceAppVersion_GetMajor(void);

uint8_t SolaceAppVersion_GetMinor(void);

uint8_t SolaceAppVersion_GetPatch(void);

#endif /* SOURCE_APPVERSION_H_ */

/**@} */
/** ************************************************************************* */

