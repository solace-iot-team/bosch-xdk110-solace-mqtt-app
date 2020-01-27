#!/bin/bash
clear
echo "Disconnect Client"
echo

source ./config.sh.include

#PATCH /msgVpns/{msgVpnName}/mqttSessions/{mqttSessionClientId},{mqttSessionVirtualRouter}

# enabled=false, true
PAYLOAD='
{
  "enabled": false
}
'

echo ----------------------------------------------
echo "payload: $PAYLOAD"
echo ----------------------------------------------

echo $PAYLOAD | curl -v -X PATCH \
  -H "Content-Type: application/json" \
  -u $SEMPV2_USER:$SEMPV2_PASSWORD \
  $SEMPV2_BASEPATH/msgVpns/$MESSAGE_VPN/mqttSessions/$XDK_DEVICE_ID,$VIRTUALROUTER \
    -d @-

echo

# enabled=false, true
PAYLOAD='
{
  "enabled": true
}
'

echo ----------------------------------------------
echo "payload: $PAYLOAD"
echo ----------------------------------------------

echo $PAYLOAD | curl -v -X PATCH \
  -H "Content-Type: application/json" \
  -u $SEMPV2_USER:$SEMPV2_PASSWORD \
  $SEMPV2_BASEPATH/msgVpns/$MESSAGE_VPN/mqttSessions/$XDK_DEVICE_ID,$VIRTUALROUTER \
    -d @-

echo
echo
# The End.
