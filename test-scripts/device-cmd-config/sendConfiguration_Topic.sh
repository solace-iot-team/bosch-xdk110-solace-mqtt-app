#!/bin/bash
echo
echo "Send Topic Configuration ..."
echo

source ./config.sh.include

EXCHANGE_ID=$(uuidgen)
TIMESTAMP=$(gdate +"%Y-%m-%dT%T.%3NZ")
MODE="topic"

CURRENT_BASE_TOPIC=$RESOURCE_CATEGORY
# if you change base topic, make sure you adapt your configuration & command scripts
NEW_BASE_TOPIC=$RESOURCE_CATEGORY

UPDATE_CONFIG_TOPIC="UPDATE/iot-control/$CURRENT_BASE_TOPIC/device/$XDK_DEVICE_ID/configuration"

# "apply": "PERSISTENT" or "TRANSIENT"

PAYLOAD='
{
  "type": "topic",
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "tags": {
    "mode": "'"$MODE"'"
  },
  "delay": 1,
  "apply": "TRANSIENT",
  "baseTopic" : "'"$NEW_BASE_TOPIC"'",
  "methodCreate": "CREATE",
  "methodUpdate": "UPDATE"
}
'

#test
echo ----------------------------------------------
echo "topic: $UPDATE_CONFIG_TOPIC"
echo "payload: $PAYLOAD"
echo ----------------------------------------------


# Solace-Delivery-Mode: [Direct | Non-Persistent | Persistent]

echo $PAYLOAD | curl \
  -H "Content-Type: application/json" \
  -H "Solace-delivery-mode: direct" \
  -X POST \
  -u $REST_USERNAME:$REST_PASSWORD \
  $REST_HOST/TOPIC/$UPDATE_CONFIG_TOPIC \
  -d @-

echo
echo
# The End.
