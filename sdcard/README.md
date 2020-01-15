## Bootstrap Configuration

The App reads it's bootstrap configuration from the file **_config.json_** in the root directory of the SD Card.

Copy **_config-template.json_** to **_config.json_** and configure:

|Item|Examples|Description|
|---------|--------------------------------|-------------------------------------------------|
|**sntpURL**|0.uk.pool.ntp.org, 0.north-america.pool.ntp.org|url of an SNTP server  |
|**sntpPort**|123|the port for the SNTP server|
|**brokerURL**|{your-service}.messaging.solace.cloud|the mqtt broker host name|
|**brokerPort**|1883|the mqtt broker port|
|**brokerUsername**|solace-cloud-client|the user name|
|**brokerPassword**|-|the password|
|**brokerKeepAliveIntervalSecs**|0 (=forever)|the number of seconds the broker keeps the connection alive without traffic|
|**wlanSSID**|-|the WLAN SSID|
|**wlanPSK**|-|the WLAN password|
|**baseTopic**|region-a/site-b/production-line-c| the resource categorization defined here is part of the topic strings. must be exactly 3 levels.  |


Copy the `` config.json `` to the SD card and insert it into the XDK.

**_Note: be careful when inserting the SD card into the XDK! When inserted incorrectly, the card may fall into the device and you will have to find two different alum keys to open it._**



-----------------------------
The End.
