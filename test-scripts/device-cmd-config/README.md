# Test Scripts for sending Commands & Configuration Messages

Sends messages via HTTP to the broker.
These are transformed by the broker to MQTT messages for the device.

Please refer to the source code documentation for the message details.

## Prerequisites

### Install ``gdate``

``gdate`` is used to add milliseconds to our timestamp.

* on MacOs: install coreutils
  - for example: ``brew install coreutils``

### Broker REST Connect Credentials

In the management console of your broker:
* tab: Connect
* open: REST
  - here you find the connection details for REST calls

### Change ``config.sh.include``

Copy ``config.sh.include.sample`` to ``config.sh.include`` and change the
parameters to reflect your broker and set-up.

## Send Command Messages

Use ``sendCommand.sh`` to send commands.
To listen to the response(s), listen to the topic strings described in the source code documentation.

## Send Configuration Messages

Use the various ``sendConfiguration_xxx.sh`` scripts to send configuration messages.
To listen to the response(s), listen to the topic strings described in the source code documentation.


------------------------------------------------------------------------------
The End.
