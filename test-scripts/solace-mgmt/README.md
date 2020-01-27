## Solace Broker Management Scripts

Some sample scripts to manage the Solace broker.

These scripts are based on the Solace SEMP V2 management interface.

The full documentation can be found [here][semp-v2].

### Adjust the parameters

- copy ``config.sh.include.sample`` to ``config.sh.include``
- change the device Id and broker SEMP V2 credentials for your broker

### Scripts

|Script| Description |
|-|-|
|**getMQTTSessions.sh**|Returns all active MQTT sessions of the VPN on the Broker.   |
|**getMQTTSession.sh**   |Returns session information for the particular device Id. The device Id is used as the client-id.   |
|**getMQTTSessionSubscriptions.sh**   |Returns all active subscriptions of the device.   |
|**patchMQTTSession.sh**   |Patches the device id session.   |
|**disconnectXDKClient.sh** | Disconnects the device by disabling and re-enabling the session. Use for testing disconnect & re-connect management of the app.   |




------------------------------------------------------------------------------
The End.

[semp-v2]: https://docs.solace.com/API-Developer-Online-Ref-Documentation/swagger-ui/config/index.html#/
