#!/bin/bash
clear
echo "Get MQTT Session Subscriptions"
echo

source ./config.sh.include

#/msgVpns/{msgVpnName}/mqttSessions/{mqttSessionClientId},{mqttSessionVirtualRouter}/subscriptions

curl -X GET \
  -u $SEMPV2_USER:$SEMPV2_PASSWORD \
  $SEMPV2_BASEPATH/msgVpns/$MESSAGE_VPN/mqttSessions/$XDK_DEVICE_ID,$VIRTUALROUTER/subscriptions

echo
echo
# The End.
