# Documentation Practices Used

- Documentation is mainly done in the source files and not in the header files
- In the header files you'll find documentation for public macros and structures
- Both, the header files and source files below to the same group
- Everything is documented, public and static functions so that caller and call graphs are rendered completely


#### Header files

````
/**
* @ingroup ModuleName
* @{
* @author $(SOLACE_APP_AUTHOR)
* @date $(SOLACE_APP_DATE)
* @file
*/

the code ...

/**@} */
/** ************************************************************************* */
````

#### Source files
````
/**
 * @defgroup ModuleName description
 * @{
 * @brief brief
 * @details details
 * @author $(SOLACE_APP_AUTHOR)
 * @date $(SOLACE_APP_DATE)
 * @file
 */

the code ...

 /**@} */
 /** ************************************************************************* */
````
#### Enums as part of external API
Enums which are part of the external API - via the MQTT messages - are documented with their actual value.
This allows for a) looking up the meaning of a value and b) for auto generation of mapping tables, value - string, in any management application.

Examples from ``XdkAppInfo.h``:
````
SOLACE_APP_MODULE_ID_MAIN = XDK_COMMON_ID_OVERFLOW, /**< 62 */

RETCODE_SOLAPP_MQTT_PUBLISH_FAILED_NO_CONNECTION, /**< 209 */

AppStatusMessage_Status_Error,  /**< 2 */

AppStatusMessage_Descr_Discarding_StillProcessingPreviousInstruction, /**< 27 */
````

#### External Interface Page

The external events / messaging interfaces are documented in a [separate .md page](./resources/docs_external_interfaces.md) and included in the generated documentation.

------------------------------------------------------------------------------
The End.
