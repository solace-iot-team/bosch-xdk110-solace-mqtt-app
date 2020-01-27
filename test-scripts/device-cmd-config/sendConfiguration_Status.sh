#!/bin/bash
echo
echo "Send Status Configuration ..."
echo

source ./config.sh.include

EXCHANGE_ID=$(uuidgen)
TIMESTAMP=$(gdate +"%Y-%m-%dT%T.%3NZ")
MODE="status"

# "periodicStatusType":"FULL_STATUS" or "SHORT_STATUS"
# "apply": "PERSISTENT" or "TRANSIENT"

PAYLOAD='
{
  "type": "status",
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "tags": {
    "mode": "'"$MODE"'"
  },
  "sendPeriodicStatus": true,
  "periodicStatusType":"FULL_STATUS",
  "periodicStatusIntervalSecs": 600,
  "qos": 0,
  "delay": 1,
  "apply": "TRANSIENT"
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
