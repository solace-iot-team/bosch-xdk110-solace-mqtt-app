# This makefile triggers the targets in the application.mk

# The default value "../../.." assumes that this makefile is placed in the
# folder xdk110/Apps/<App Folder> where the BCDS_BASE_DIR is the parent of
# the xdk110 folder.
BCDS_BASE_DIR ?= ../../..

# Macro to define Start-up method. change this macro to "CUSTOM_STARTUP" to have custom start-up.
export BCDS_SYSTEM_STARTUP_METHOD = DEFAULT_STARTUP
export BCDS_APP_NAME = SolaceMQTTApp
export BCDS_APP_DIR = $(CURDIR)
export BCDS_APP_SOURCE_DIR = $(BCDS_APP_DIR)/source
export BCDS_FREERTOS_INCLUDE_AWS=0

# DEBUG FLAGS for the modules
#export SOLACE_CFLAGS_DEBUG_APP_CONTROLLER = -DDEBUG_APP_CONTROLLER
#export SOLACE_CFLAGS_DEBUG_APP_CONFIG = -DDEBUG_APP_CONFIG
#export SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_PUBLISH = -DDEBUG_APP_TELEMETRY_PUBLISH
#export SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_PUBLISH_EVERY_MESSAGE = -DDEBUG_APP_TELEMETRY_PUBLISH_EVERY_MESSAGE
#export SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_SAMPLING = -DDEBUG_APP_TELEMETRY_SAMPLING
#export SOLACE_CFLAGS_DEBUG_APP_RUNTIME_CONFIG = -DDEBUG_APP_RUNTIME_CONFIG
#export SOLACE_CFLAGS_DEBUG_APP_CMD_CTRL = -DDEBUG_APP_CMD_CTRL
#export SOLACE_CFLAGS_DEBUG_APP_MQTT = -DDEBUG_APP_MQTT
export SOLACE_CFLAGS_DEBUG_APP_XDK_MQTT = -DDEBUG_APP_XDK_MQTT
#export SOLACE_CFLAGS_DEBUG_APP_XDK_MQTT_EVERY_PUBLISHED_DATA_CALLBACK = -DDEBUG_APP_XDK_MQTT_EVERY_PUBLISHED_DATA_CALLBACK
#export SOLACE_CFLAGS_DEBUG_APP_BUTTONS = -DDEBUG_APP_BUTTONS
#export SOLACE_CFLAGS_DEBUG_APP_STATUS = -DDEBUG_APP_STATUS

#Please refer BCDS_CFLAGS_COMMON variable in application.mk file
#and if any addition flags required then add that flags only in the below macro
#export BCDS_CFLAGS_COMMON =

#set the final flags
export BCDS_CFLAGS_COMMON = \
	$(SOLACE_CFLAGS_DEBUG_APP_CONTROLLER) \
	$(SOLACE_CFLAGS_DEBUG_APP_CONFIG) \
	$(SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_SAMPLING) \
	$(SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_PUBLISH) \
	$(SOLACE_CFLAGS_DEBUG_APP_TELEMETRY_PUBLISH_EVERY_MESSAGE) \
	$(SOLACE_CFLAGS_DEBUG_APP_RUNTIME_CONFIG) \
	$(SOLACE_CFLAGS_DEBUG_APP_CMD_CTRL) \
	$(SOLACE_CFLAGS_DEBUG_APP_XDK_MQTT) \
	$(SOLACE_CFLAGS_DEBUG_APP_XDK_MQTT_EVERY_PUBLISHED_DATA_CALLBACK) \
	$(SOLACE_CFLAGS_DEBUG_APP_MQTT) \
	$(SOLACE_CFLAGS_DEBUG_APP_BUTTONS) \
	$(SOLACE_CFLAGS_DEBUG_APP_STATUS) 



# Macro for server cetificate which is used for secure communication.
# User can provide custom server certificate as well.
# By default the test.mosquito.org MQTT open source broker certificate is configured.
#export XDK_APP_CERTIFICATE_NAME = Solace // unused

#Below settings are done for optimized build.Unused common code is disabled to reduce the build time
export XDK_FEATURE_SET='SELECT'
#Enabled feature macros for common code
export XDK_CONNECTIVITY_LED=1
export XDK_SENSOR_BUTTON=1
export XDK_UTILITY_STORAGE=1
export XDK_CONNECTIVITY_WLAN=1
export XDK_SENSOR_SENSOR=1
export XDK_UTILITY_SERVALPAL=1
export XDK_UTILITY_SNTP=1

export XDK_CONNECTIVITY_MQTT=0
export XDK_CONNECTIVITY_BLE=0
export XDK_CONNECTIVITY_HTTPRESTCLIENT=0
export XDK_CONNECTIVITY_LORA=0
export XDK_CONNECTIVITY_LWM2M=0
export XDK_CONNECTIVITY_UDP=0
export XDK_SENSOR_VIRTUALSENSOR=0
export XDK_SENSOR_EXTERNALSENSOR=0

#end of settings related to optimized build

# This variable should fully specify the build configuration of the Serval
# Stack library with regards the enabled and disabled features for the HTTPS Using TLS.
export SERVAL_TLS_MBEDTLS=0
export SERVAL_ENABLE_TLS_CLIENT=0
export SERVAL_ENABLE_TLS_ECC=1
export SERVAL_ENABLE_TLS_PSK=0
export SERVAL_MAX_NUM_MESSAGES=8
export SERVAL_MAX_SIZE_APP_PACKET=900
export SERVAL_ENABLE_TLS=0

export XDK_MBEDTLS_PARSE_INFO=0

#List all the application header file under variable BCDS_XDK_INCLUDES
export BCDS_XDK_INCLUDES = \
	-I $(BCDS_BASE_DIR)/xdk110/Common/source/Connectivity

#List all the application source file under variable BCDS_XDK_APP_SOURCE_FILES in a similar pattern as below
export BCDS_XDK_APP_SOURCE_FILES = \
	$(wildcard $(BCDS_APP_SOURCE_DIR)/*.c)

.PHONY: clean debug release flash_debug_bin flash_release_bin

clean:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk clean

debug:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk debug

release:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk release

clean_Libraries:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk clean_libraries

flash_debug_bin:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk flash_debug_bin

flash_release_bin:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk flash_release_bin

cdt:
	$(MAKE) -C $(BCDS_BASE_DIR)/xdk110/Common -f application.mk cdt
