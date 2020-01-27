/*
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for application development by Licensee. 
* Fitness and suitability of the example code for any use within application developed by Licensee need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
* Licensee shall be responsible for conducting the development of its applications as well as integration of parts of the example code into such applications, taking into account the state of the art of technology and any statutory regulations and provisions applicable for such applications. Compliance with the functional system requirements and testing there of (including validation of information/data security aspects and functional safety) and release shall be solely incumbent upon Licensee. 
* For the avoidance of doubt, Licensee shall be responsible and fully liable for the applications and any distribution of such applications into the market.
* 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are 
* met:
* 
*     (1) Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer. 
* 
*     (2) Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.  
*     
*     (3)The name of the author may not be used to
*     endorse or promote products derived from this software without
*     specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
*  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
*  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
*  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*/
/*----------------------------------------------------------------------------*/

/**
 * @defgroup Main Main
 * @{
 *
 * @brief The Main.
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
#define BCDS_MODULE_ID SOLACE_APP_MODULE_ID_MAIN

/* system header files */
#include <stdio.h>
#include "BCDS_Basics.h"

/* additional interface header files */
#include "XdkSystemStartup.h"
#include "BCDS_Assert.h"
#include "AppController.h"
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"
#include "task.h"
/* own header files */
#include "AppStatus.h"

/* global variables ********************************************************* */
static CmdProcessor_T MainCmdProcessor; /**< command processor passed to @ref AppController module. */


/**
 * @brief The main() function invoked at start-up.
 * @details Initializes the Retcode module with @ref AppStatus_ErrorHandlingFunc()
 * @details Innitializes the command processor for the @ref AppController module, enqueues the @ref AppController_Init() function, and starts the scheduler.
 *
 * @exception main Retcode_RaiseError() if @ref AppController_Init() could not be enqueued.
 * @exception main assert(false) if scheduler could not be started.
 */
int main(void) {

	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) retcode = AppStatus_InitErrorHandling();

	if (RETCODE_OK == retcode) retcode = Retcode_Initialize(AppStatus_ErrorHandlingFunc);

	if (RETCODE_OK == retcode) retcode = systemStartup();

    if (RETCODE_OK == retcode) {
        retcode = CmdProcessor_Initialize(&MainCmdProcessor, (char *) "AppControllerProcessor", APP_CONTROLLER_PROCESSOR_PRIORITY, APP_CONTROLLER_PROCESSOR_STACK_SIZE, APP_CONTROLLER_PROCESSOR_QUEUE_LEN);
    }

    if (RETCODE_OK == retcode) {
        /* Here we enqueue the application initialization into the command
         * processor, such that the initialization function will be invoked
         * once the RTOS scheduler is started below.
         */
        retcode = CmdProcessor_Enqueue(&MainCmdProcessor, AppController_Init, &MainCmdProcessor, UINT32_C(0));
    }

    if (RETCODE_OK == retcode) {
        /* start scheduler */
        vTaskStartScheduler();
        /* Code must not reach here since the OS must take control. If not, we will assert. */
        assert(false);
    } else {
        Retcode_RaiseError(retcode);
        printf("main : XDK System Startup failed.\r\n");
    }
    assert(false);
}

/**@} */
 /** ************************************************************************* */


