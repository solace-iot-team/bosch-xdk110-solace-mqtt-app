#!/bin/bash
echo
echo "Send Telemetry Configuration ..."
echo

source ./config.sh.include


EXCHANGE_ID=$(uuidgen)
TIMESTAMP=$(gdate +"%Y-%m-%dT%T.%3NZ")
MODE="telemetry"

# "apply": "PERSISTENT" or "TRANSIENT"
# "payloadFormat" : "V1_JSON_VERBOSE" or "V1_JSON_COMPACT"
# sensors:
#   "humidity",
#   "light",
#   "temperature",
#   "accelerator",
#   "gyroscope",
#   "magnetometer"
#
PAYLOAD='
{
  "type": "telemetry",
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "tags": {
    "mode": "'"$MODE"'"
  },
  "delay": 1,
  "apply": "TRANSIENT",
  "activateAtBootTime": true,
  "eventFrequencyPerSec": 1,
  "samplesPerEvent": 2,
  "qos": 0,
  "payloadFormat" : "V1_JSON_COMPACT",
  "sensors": [
    "humidity",
    "light",
    "temperature",
    "accelerator",
    "gyroscope",
    "magnetometer"
  ]
}
'

#test
echo ----------------------------------------------
echo "topic: $UPDATE_CONFIG_TOPIC"
echo "payload: $PAYLOAD"
echo ----------------------------------------------


# Solace-Delivery-Mode: [Direct | Non-Persistent | Persistent]

echo $PAYLOAD | curl  \
  -H "Content-Type: application/json" \
  -H "Solace-delivery-mode: direct" \
  -X POST \
  -u $REST_USERNAME:$REST_PASSWORD \
  $REST_HOST/TOPIC/$UPDATE_CONFIG_TOPIC \
  -d @-

echo
echo
# The End.
