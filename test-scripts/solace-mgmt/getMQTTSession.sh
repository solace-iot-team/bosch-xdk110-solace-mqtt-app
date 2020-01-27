#!/bin/bash
clear
echo "Get MQTT Session for 1 client id"
echo

source ./config.sh.include

#/msgVpns/{msgVpnName}/mqttSessions/{mqttSessionClientId},{mqttSessionVirtualRouter}

curl -X GET \
  -u $SEMPV2_USER:$SEMPV2_PASSWORD \
  $SEMPV2_BASEPATH/msgVpns/$MESSAGE_VPN/mqttSessions/$XDK_DEVICE_ID,$VIRTUALROUTER

echo
echo
# The End.
