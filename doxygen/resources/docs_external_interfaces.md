# External Interfaces

## Topic Patterns

Pattern:
````
{method}/{representation}/{base-topic}/{version}/{resource-categorization}/{resource}/{id}/{aspect}
````

Used here:

|Topic Part|Examples|Description|
|---------|--------------------------------|-------------------------------------------------|
|{method}|CREATE, UPDATE|denotes whether the message is supposed to be treated as an update or as a newly created message  |
|{representation}|omitted|JSON  |
|{base-topic}|iot-event, iot-control|similar to 'baseURL' in the REST world  |
|{version}|omitted  ||
|{resource-categorization}|split into {region}/{site}/{sub-site}| 3 part categorization|
|{resource}|device|static  |
|{deviceId}|12323ef45|the device Id  |
|{aspect}|command, configuration, metrics, status|the aspect of the service / message  |


## General Type Formats

### Timestamp

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|timestamp|[string][@ref APP_TIMESTAMP_STRING_FORMAT]|timestamp of when the event / message / response was generated<br/><b>Example:</b>@code"timestamp": "2019-08-23T10:53:05.406Z"@endcode<br/>@see AppTimestamp|

### Status, Retcodes, Module Ids

@see XdkAppInfo

Status codes: @ref AppStatusMessage_StatusCode_T

Status Description Codes: @ref AppStatusMessage_DescrCode_T

Module Ids: @ref Solace_App_ModuleID_E

Retcodes: @ref Solace_App_Retcode_E

## Configuration & Commands

The interface is implemented in the module @ref AppCmdCtrl.

It provides a minimum level of protection against too many commands in quick succession. See
@ref appCmdCtrl_SubscriptionCallBack().

### General Responses to Commands & Configuration

Topic:
````
UPDATE/iot-control/{region}/{site}/{sub-site}/device/{deviceId}/status
````

**Response Structure:**

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|deviceId|[string]|the device id   |
|timestamp||when response was generated|
|exchangeId|[string]|the exchangeId from the initial command/configuration|
|statusCode|[number][@ref AppStatusMessage_StatusCode_T]| the status code  |
|status|[string][SUCCESS, FAILED] |status string |
|requestType|[string][CONFIGURATION, COMMAND]|the request type string |
|descrCode|[number][@ref AppStatusMessage_DescrCode_T]|the description code   |
|tags|[optional][object]|pass-through from incoming message, omitted if it was not present|
|items|[array of objects][originalPayload][object]|the original payload   |


**Example Response (success):**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-20T16:07:46.430Z",
  "exchangeId": "73EDC164-65ED-47E5-A2D4-CE65C668A18D",
  "statusCode": 3,
  "status": "SUCCESS",
  "requestType": "COMMAND",
  "descrCode": 0,
  "tags": {
    "my-tag": "my-tag-value"
  }
}
````
**Example Response (failure):**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-20T17:41:36.762Z",
  "exchangeId": "AD9508F1-8F4C-4714-8F48-3BED5A663359",
  "statusCode": 4,
  "status": "FAILED",
  "requestType": "COMMAND",
  "descrCode": 27,
  "items": [
    {
      "originalPayload": {
        "timestamp": "2020-01-20T17:41:36.794Z",
        "exchangeId": "AD9508F1-8F4C-4714-8F48-3BED5A663359",
        "command": "SUSPEND_TELEMETRY",
        "delay": 1,
        "tags": {
          "my-tag": "my-tag-value"
        }
      }
    }
  ],
  "tags": {
    "my-tag": "my-tag-value"
  }
}
````
### Configuration

Update the configuration for the device and return either a success or failed status.
The failed status contains the failure reason.
An exchangeId is used to correlate the command message with the response(s).

#### Configuration Topics

@see appCmdCtrl_PubSubSetup()

||Pattern / Description|
|--|--|
|Pattern|``UPDATE/iot-control/{region}/{site}/{sub-site}/device/{deviceId}/configuration``   |
| |Configuration for a single device.   |
|Pattern|``UPDATE/iot-control/{region}/{site}/{sub-site}/device/command``   |
| |Configuration for all devices within the resource category: region/site/sub-site   |
|Pattern|``UPDATE/iot-control/{region}/{site}/device/command``|
| |Configuration for all devices within the resource category: region/site   |
|Pattern|``UPDATE/iot-control/{region}/device/command``|
| |Configuration for all devices within the resource category: region |


#### The Runtime Config File

The runtime config file name on the SD Card: @ref RT_CFG_FILE_NAME.

#### General Configuration Elements

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|type|[string][telemetry, topic, status, mqttBrokerConnection]|the configuration type |
|timestamp|[optional]|not used by XDK app, use to e.g. create a log / audit|
|exchangeId|[mandatory][string]|Unique identifier (e.g. UUID) for this command message exchange, status messages will include this id so they can be correlated with the command.|
|delay|[optional][number][default=1,min=0,max=10][seconds]|number of seconds to wait before applying the new configuration.|
|tags|[optional][object]|not used by the XDK app, but included in the status response as-is. |
|apply|[optional][string][@ref APP_RT_CFG_APPLY_TRANSIENT, @ref APP_RT_CFG_APPLY_PERSISTENT][default=@ref APP_RT_CFG_APPLY_TRANSIENT]|flag to either persist the configuration or keep it in memory only |


#### Telemetry Configuration

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|activateAtBootTime|[boolean][true, false]|activate telemetry at boot time or not|
|eventFrequencyPerSec|[number][seconds]|number of telemetry events to send per second|
|samplesPerEvent|[number]|number of sensor samples per event|
|qos|[optional][number][0, 1][default=0]|the qos for the telemetry events|
|payloadFormat|[optional][string][[@ref APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR, @ref APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_COMPACT_STR][default=@ref APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR]|payload format|
|sensors|[mandatory][array of strings][min 1 element]|selection of sensor values to include in the telemetry event   |

**Sensor Array**

|Value|Description|
|-|-|
|"humidity"|   |
|"light"|   |
|"temperature"|   |
|"accelerator"|   |
|"gyroscope"|   |
|"magnetometer"|   |

**Example Telemetry Configuration Payload:**
````
{
  "type": "telemetry",
  "timestamp": "2020-01-20T18:38:33.643Z",
  "exchangeId": "8CC80F25-59FB-42DD-BD69-F72BA6BEBED7",
  "tags": {
    "mode": "telemetry"
  },
  "delay": 1,
  "apply": "PERSISTENT",
  "activateAtBootTime": true,
  "eventFrequencyPerSec": 1,
  "samplesPerEvent": 2,
  "qos": 1,
  "payloadFormat" : "V1_JSON_VERBOSE",
  "sensors": [
    "humidity",
    "light",
    "temperature",
    "accelerator",
    "gyroscope",
    "magnetometer"
  ]
}
````

#### Status Configuration

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|type|[mandatory][string]["status"]|type=status|
|sendPeriodicStatus|[mandatory][boolean][true, false]|flag to send periodic status or not|
|periodicStatusType|[mandatory][string][@ref APP_RT_CFG_STATUS_PERIODIC_TYPE_FULL, @ref APP_RT_CFG_STATUS_PERIODIC_TYPE_SHORT]|type of status to send|
|qos|[optional][number][0, 1][default=@ref APP_RT_CFG_DEFAULT_STATUS_QOS]|the qos for the status events|
|periodicStatusIntervalSecs|[mandatory][number][min=@ref APP_RT_CFG_STATUS_MIN_INTERVAL_SECS][seconds]|the interval in seconds to send the period status event(s)|

**Example Status Configuration Payload:**
````
{
  "type": "status",
  "timestamp": "2020-01-27T11:11:39.070Z",
  "exchangeId": "9A25D1B1-E86B-4D57-B523-D5B4526E1F43",
  "tags": {
    "mode": "status"
  },
  "sendPeriodicStatus": true,
  "periodicStatusType":"FULL_STATUS",
  "periodicStatusIntervalSecs": 600,
  "qos": 0,
  "delay": 1,
  "apply": "TRANSIENT"
}
````

#### Topic Configuration

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|type|[mandatory][string]["topic"]|type=topic|
|baseTopic|[mandatory][string][three level topic sub-string]|the resource categorization, 3 levels|
|methodCreate|[optional][string][default=@ref APP_RT_CFG_DEFAULT_TOPIC_METHOD_CREATE]|the 'create' verb|
|methodUpdate|[optional][string][default=@ref APP_RT_CFG_DEFAULT_TOPIC_METHOD_UPDATE]|the 'update' verb|

**Example Topic Configuration Payload:**
````
payload:
{
  "type": "topic",
  "timestamp": "2020-01-27T11:28:56.606Z",
  "exchangeId": "5078BB46-F1DB-4EA6-8D9C-B08A00711A61",
  "tags": {
    "mode": "topic"
  },
  "delay": 1,
  "apply": "TRANSIENT",
  "baseTopic" : "region/site/sub-site",
  "methodCreate": "CREATE",
  "methodUpdate": "UPDATE"
}
````


### Commands

Commands are instructions that the app executes and return either a success or failed status.
The failed status contains the failure reason.
An exchangeId is used to correlate the command message with the response(s).

#### Command Topics

@see appCmdCtrl_PubSubSetup()

||Pattern / Description|
|--|--|
|Pattern|``CREATE/iot-control/{region}/{site}/{sub-site}/device/{deviceId}/command``   |
| |Command for a single device.   |
|Pattern|``CREATE/iot-control/{region}/{site}/{sub-site}/device/command``   |
| |Command for all devices within the resource category: region/site/sub-site   |
|Pattern|``CREATE/iot-control/{region}/{site}/device/command``|
| |Command for all devices within the resource category: region/site   |
|Pattern|``CREATE/iot-control/{region}/device/command``|
| |Command for all devices within the resource category: region |


#### Command Payload Structure

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|timestamp|[optional]|not used by XDK app, use to e.g. create a log / audit|
|exchangeId|[mandatory][string]|Unique identifier (e.g. UUID) for this command message exchange, status messages will include this id so they can be correlated with the command.|
|command|[mandatory][string][value from Command List]|the command to execute|
|delay|[optional][number][default=1,min=0,max=10][seconds]|number of seconds to wait before executing the command.|
|tags|[optional][object]|not used by the XDK app, but included in the status response as-is. |

**Example Payload:**
````
{
  "timestamp": "2020-01-20T13:55:50.652Z",
  "exchangeId": "347D8C62-DBE0-442D-B350-4A39C0FA7214",
  "command": "RESUME_TELEMETRY",
  "delay": 1,
  "tags": {
    "my-tag" : "my-tag-value"
  }
}
````

#### Command List

|Command|Response|
|-|-|
|@ref COMMAND_SUSPEND_TELEMETRY | single response: success or failure|
|@ref COMMAND_RESUME_TELEMETRY | single response: success or failure |
|@ref COMMAND_REBOOT | single response before command is executed: success   |
|@ref COMMAND_DELETE_RUNTIME_CONFIG_FILE | single response: success or failure    |
|@ref COMMAND_PERSIST_ACTIVE_CONFIG | single response: success or failure    |
|@ref COMMAND_SEND_FULL_STATUS| multi-part response, see @ref appStatus_SendFullStatus()|
|@ref COMMAND_SEND_SHORT_STATUS| see @ref appStatus_SendShortStatus()|
|@ref COMMAND_SEND_VERSION_INFO| see @ref AppStatus_SendVersionInfo()  |
|@ref COMMAND_SEND_ACTIVE_TELEMETRY_PARAMS | see @ref AppStatus_SendActiveTelemetryParams()   |
|@ref COMMAND_SEND_ACTIVE_RUNTIME_CONFIG| see @ref AppRuntimeConfig_SendActiveConfig()  |
|@ref COMMAND_SEND_RUNTIME_CONFIG_FILE   | see @ref AppRuntimeConfig_SendFile()  |
|@ref COMMAND_TRIGGER_SAMPLE_ERROR					| trigger a sample error  |
|@ref COMMAND_TRIGGER_SAMPLE_FATAL_ERROR     | trigger a sample fatal error |

## Bootstrap Config

@see AppConfig

The filename on the SD Card: @ref APP_CONFIG_FILENAME

**Example:**
@code
  {
	"sntpURL":"change_me: e.g. 0.uk.pool.ntp.org or 0.north-america.pool.ntp.org",
	"sntpPort": 123,
	"brokerURL":"change_me: MQTT Host",
	"brokerPort": MQTT port number,
	"brokerUsername":"change_me, MQTT username",
	"brokerPassword":"change_me, MQTT password",
	"brokerCleanSession": false,
	"brokerSecureConnection": false,
	"brokerKeepAliveIntervalSecs": 60
	"wlanSSID":"change_me: SSID",
	"wlanPSK":"change_me: password",
	"baseTopic": "change_me, 3 levels, e.g. {region}/{site}/{production-line}"
  }
@endcode


## Telemetry Events

### Sensor Events
The sensor events are controlled by the Telemetry Configuration.
The event structure is a JSON Array, even if it only contains 1 sample.

Topic:
````
CREATE/iot-event/{region}/{site}/{sub-site}/device/{deviceId}/metrics
````

**Example Sensor Event**
- @ref APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_VERBOSE_STR
- all sensors are included
````
[
  {
    "timestamp": "2020-01-27T09:51:47.047Z",
    "deviceId": "24d11f0358cd5d9a",
    "humidity": 57,
    "light": 103680,
    "temperature": 17.071,
    "acceleratorX": -2,
    "acceleratorY": 1,
    "acceleratorZ": 992,
    "gyroX": 854,
    "gyroY": -549,
    "gyroZ": -5612,
    "magR": 6427,
    "magX": -44,
    "magY": -28,
    "magZ": -40
  },
  {
    "timestamp": "2020-01-27T09:51:47.547Z",
    "deviceId": "24d11f0358cd5d9a",
    "humidity": 57,
    "light": 103680,
    "temperature": 17.081,
    "acceleratorX": 0,
    "acceleratorY": 3,
    "acceleratorZ": 990,
    "gyroX": 915,
    "gyroY": -671,
    "gyroZ": -5734,
    "magR": 6425,
    "magX": -42,
    "magY": -27,
    "magZ": -38
  }
]
````

**Example Sensor Event**
- @ref APP_RT_CFG_TELEMETRY_PAYLOAD_FORMAT_V1_JSON_COMPACT_STR
- all sensors are included
````
[
  {
    "ts": "2020-01-27T09:59:04.511Z",
    "id": "24d11f0358cd5d9a",
    "h": 48,
    "l": 100800,
    "t": 20.211,
    "aX": -4,
    "aY": 1,
    "aZ": 986,
    "gX": 854,
    "gY": -610,
    "gZ": -5612,
    "mR": 6299,
    "mX": -42,
    "mY": -29,
    "mZ": -40
  },
  {
    "ts": "2020-01-27T09:59:05.011Z",
    "id": "24d11f0358cd5d9a",
    "h": 48,
    "l": 100800,
    "t": 20.211,
    "aX": -2,
    "aY": 4,
    "aZ": 989,
    "gX": 854,
    "gY": -671,
    "gZ": -5673,
    "mR": 6299,
    "mX": -42,
    "mY": -28,
    "mZ": -38
  }
]
````

### Button Events
@see AppButtons
**Topic:**
````
CREATE/iot-event/{region}/{site}/{sub-site}/device/{deviceId}/button
````

**Payload:**

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|timestamp|[always]||
|deviceId|[always][string]||
|buttonNumber|[always][number][1,2]|the button number pressed / released |
|event|[always][string][@ref PAYLOAD_VALUE_BUTTON_PRESSED, @ref PAYLOAD_VALUE_BUTTON_RELEASED]|the action|


**Example Button Events**
````
{
  "timestamp": "2020-01-27T10:05:15.606Z",
  "deviceId": "24d11f0358cd5d9a",
  "buttonNumber": 1,
  "event": "PRESSED"
}

{
  "timestamp": "2020-01-27T10:05:15.794Z",
  "deviceId": "24d11f0358cd5d9a",
  "buttonNumber": 1,
  "event": "RELEASED"
}
````

## Status Events
Sent either as a response to a command or as regular status events.

### Short Status

**Example Short Status**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:39:14.749Z",
  "exchangeId": "A7467B66-81EF-4785-BD0B-6C900AF734D7",
  "statusCode": 0,
  "descrCode": 23,
  "items": [
    {
      "stats": {
        "bootTimestamp": "2020-01-27T10:09:01.590Z",
        "bootBatteryVoltage": 4377,
        "currentBatteryVoltage": 4365,
        "mqttBrokerDisconnectCounter": 0,
        "wlanDisconnectCounter": 0,
        "statusSendFailedCounter": 0,
        "telemetrySendFailedCounter": 0,
        "telemetrySendTooSlowCounter": 172,
        "telemetrySamplingTooSlowCounter": 0,
        "retcodeRaisedErrorCounter": 0
      }
    },
    {
      "activeTelemetryRTParams": {
        "numberOfSamplesPerEvent": 2,
        "publishPeriodcityMillis": 1000,
        "samplingPeriodicityMillis": 500
      }
    }
  ]
}
````


### Full Status

**Example Full Status Sequence**

**Part 1:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:02.980Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 1,
  "statusCode": 0,
  "descrCode": 15,
  "details": "GENERAL",
  "items": [
    {
      "stats": {
        "bootTimestamp": "2020-01-27T10:09:01.590Z",
        "bootBatteryVoltage": 4377,
        "currentBatteryVoltage": 4368,
        "mqttBrokerDisconnectCounter": 0,
        "wlanDisconnectCounter": 0,
        "statusSendFailedCounter": 0,
        "telemetrySendFailedCounter": 0,
        "telemetrySendTooSlowCounter": 178,
        "telemetrySamplingTooSlowCounter": 0,
        "retcodeRaisedErrorCounter": 0
      }
    },
    {
      "versions": {
        "XDK_Version_Major": 3,
        "XDK_Version_Minor": 6,
        "XDK_Version_Patch": 0,
        "Solace_App_Version_Major": 2,
        "Solace_App_Version_Minor": 0,
        "Solace_App_Version_Patch": 0
      }
    }
  ]
}
````

**Part 2:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:02.986Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 2,
  "statusCode": 0,
  "descrCode": 15,
  "items": [
    {
      "topicConfig": {
        "received": {
          "timestamp": "2020-01-27T11:28:56.606Z",
          "exchangeId": "5078BB46-F1DB-4EA6-8D9C-B08A00711A61",
          "tags": {
            "mode": "topic"
          },
          "delay": 1,
          "apply": "TRANSIENT",
          "baseTopic": "region/site/sub-site",
          "methodCreate": "CREATE",
          "methodUpdate": "UPDATE"
        }
      }
    }
  ]
}
````

**Part 3:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:02.991Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 3,
  "statusCode": 0,
  "descrCode": 15,
  "items": [
    {
      "mqttBrokerConnectionConfig": {
        "received": {
          "timestamp": "2020-01-20T16:07:31.213Z",
          "exchangeId": "default-exchange-id",
          "tags": {
            "mode": "APP_DEFAULT_MODE"
          },
          "delay": 1,
          "apply": "TRANSIENT",
          "brokerURL": "mr1dns3dpz5u9l.messaging.solace.cloud",
          "brokerPort": 1883,
          "brokerUsername": "solace-cloud-client",
          "brokerPassword": "bppg33clnljgqujajvqodor1vs",
          "cleanSession": false,
          "secureConnection": false,
          "keepAliveIntervalSecs": 0
        }
      }
    }
  ]
}
````

**Part 4:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:02.997Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 4,
  "statusCode": 0,
  "descrCode": 15,
  "items": [
    {
      "statusConfig": {
        "received": {
          "timestamp": "2020-01-27T11:11:39.070Z",
          "exchangeId": "9A25D1B1-E86B-4D57-B523-D5B4526E1F43",
          "tags": {
            "mode": "status"
          },
          "delay": 1,
          "apply": "TRANSIENT",
          "sendPeriodicStatus": true,
          "periodicStatusIntervalSecs": 600,
          "periodicStatusType": "FULL_STATUS",
          "qos": 0
        }
      }
    }
  ]
}
````

**Part 5:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:03.003Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 5,
  "statusCode": 0,
  "descrCode": 15,
  "items": [
    {
      "activeTelemetryRTParams": {
        "numberOfSamplesPerEvent": 2,
        "publishPeriodcityMillis": 1000,
        "samplingPeriodicityMillis": 500
      }
    }
  ]
}
````

**Part 6:**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T11:42:03.006Z",
  "exchangeId": "E827D6E9-0B89-465B-8992-973CC96979EF",
  "isManyParts": true,
  "totalNumberOfParts": 6,
  "thisPartNumber": 6,
  "statusCode": 0,
  "descrCode": 15,
  "items": [
    {
      "targetTelemetryConfig": {
        "received": {
          "timestamp": "2020-01-27T09:58:55.034Z",
          "exchangeId": "FDB99BE4-95F5-411C-B571-497380CAB10B",
          "tags": {
            "mode": "telemetry"
          },
          "delay": 1,
          "apply": "PERSISTENT",
          "activateAtBootTime": true,
          "sensorsEnable": "ALL",
          "eventFrequencyPerSec": 1,
          "samplesPerEvent": 2,
          "qos": 0,
          "payloadFormat": "V1_JSON_COMPACT",
          "sensors": [
            "light",
            "accelerator",
            "gyroscope",
            "magnetometer",
            "humidity",
            "temperature"
          ]
        }
      }
    }
  ]
}
````

### Error Messages

**Topic:**
The standard 'status' topic.

**Payload:**

|Element|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|timestamp|[timestamp]||
|deviceId|[string]||
|statusCode|[number][[@ref AppStatusMessage_Status_Error]]| the status code, see @ref AppStatusMessage_StatusCode_T|
|descrCode|[number][@ref AppStatusMessage_Descr_InternalAppError]| the status description code, see @ref AppStatusMessage_DescrCode_T   |
|items|[array with 1 object]["appError"]| the details of the error|

|Element: "appError"|Type/Format/Values/Unit|Description|
|---------|---------------------|----------|
|tickCountRaised|[number]| the internal tick count of the XDK when the error was raised |
|severity|[string]| see @ref appStatus_getSeverityStr()|
|package|[string]| the package string, see @ref appStatus_getPackageIdStr()|
|module|[string]| the module string, see @ref appStatus_getModuleIdStr()   |
|packageId|[number]| the package id, @ref SOLACE_APP_PACKAGE_ID or XDK package Id|
|moduleId|[number]| the module id, @ref Solace_App_ModuleID_E or XDK module Id   |
|severityId|[number]| the severity code, see RETCODE_SEVERITY_XXXX   |
|code|[number]|the retcode, @ref Solace_App_Retcode_E or XDK module retcode   |

**Example 'ERROR':**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T12:20:15.284Z",
  "statusCode": 2,
  "descrCode": 21,
  "items": [
    {
      "appError": {
        "tickCountRaised": 84190,
        "severity": "RETCODE_SEVERITY_ERROR",
        "package": "SOLACE-APP",
        "module": "SOLACE_APP_MODULE_ID_APP_CONTROLLER",
        "packageId": 55,
        "moduleId": 64,
        "severityId": 2,
        "code": 232
      }
    }
  ]
}
````

**Example 'FATAL ERROR':**
````
{
  "deviceId": "24d11f0358cd5d9a",
  "timestamp": "2020-01-27T12:21:55.670Z",
  "statusCode": 2,
  "descrCode": 21,
  "items": [
    {
      "appError": {
        "tickCountRaised": 184576,
        "severity": "RETCODE_SEVERITY_FATAL",
        "package": "SOLACE-APP",
        "module": "SOLACE_APP_MODULE_ID_APP_CONTROLLER",
        "packageId": 55,
        "moduleId": 64,
        "severityId": 1,
        "code": 232
      }
    }
  ]
}
````


---
The End.
