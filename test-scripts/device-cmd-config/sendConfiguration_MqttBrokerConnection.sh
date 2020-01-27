#!/bin/bash
echo
echo "Send Mqtt Broker Connection Configuration ..."
echo

source ./config.sh.include


EXCHANGE_ID=$(uuidgen)
TIMESTAMP=$(gdate +"%Y-%m-%dT%T.%3NZ")


# "apply": "PERSISTENT" or "TRANSIENT"

PAYLOAD='
{
  "type": "mqttBrokerConnection",
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "tags": {
    "mode": "MY-TAG"
  },
  "delay": 1,
  "apply": "TRANSIENT",
  "brokerURL": "{url}",
  "brokerPort": 1234,
  "brokerUsername": "{username}",
  "brokerPassword": "{password}",
  "keepAliveIntervalSecs": 60,
  "secureConnection": false,
  "cleanSession": false
}
'

#test
echo ----------------------------------------------
echo "topic: $UPDATE_CONFIG_TOPIC"
echo "payload: $PAYLOAD"
echo ----------------------------------------------


# Solace-Delivery-Mode: [Direct | Non-Persistent | Persistent]

echo $PAYLOAD | curl -v \
  -H "Content-Type: application/json" \
  -H "Solace-delivery-mode: direct" \
  -X POST \
  -u $REST_USERNAME:$REST_PASSWORD \
  $REST_HOST/TOPIC/$UPDATE_CONFIG_TOPIC \
  -d @-

echo
echo
# The End.
