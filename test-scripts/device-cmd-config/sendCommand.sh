#!/bin/bash
echo
echo "Send Command ..."
echo

source ./config.sh.include

# commands:
# SUSPEND_TELEMETRY
# RESUME_TELEMETRY
# SEND_SHORT_STATUS
# SEND_FULL_STATUS
# SEND_ACTIVE_TELEMETRY_PARAMS
# SEND_ACTIVE_RUNTIME_CONFIG
# SEND_RUNTIME_CONFIG_FILE
# DELETE_RUNTIME_CONFIG_FILE
# PERSIST_ACTIVE_CONFIG
# REBOOT
# TRIGGER_SAMPLE_ERROR
# TRIGGER_SAMPLE_FATAL_ERROR
COMMAND="TRIGGER_SAMPLE_FATAL_ERROR"


EXCHANGE_ID=$(uuidgen)
TIMESTAMP=$(gdate +"%Y-%m-%dT%T.%3NZ")

PAYLOAD='
{
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "command": "'"$COMMAND"'",
  "delay": 1,
  "tags": {
    "my-tag" : "my-tag-value"
  }
}
'

#test
echo ----------------------------------------------
echo "topic: $CREATE_COMMAND_TOPIC"
echo "payload: $PAYLOAD"
echo ----------------------------------------------


# Solace-Delivery-Mode: [Direct | Non-Persistent | Persistent]

echo $PAYLOAD | curl -v \
  -H "Content-Type: application/json" \
  -H "Solace-delivery-mode: direct" \
  -X POST \
  -u $REST_USERNAME:$REST_PASSWORD \
  $REST_HOST/TOPIC/$CREATE_COMMAND_TOPIC \
  -d @-

echo
echo
# The End.
