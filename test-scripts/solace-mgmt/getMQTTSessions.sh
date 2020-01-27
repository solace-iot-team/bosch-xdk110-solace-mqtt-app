#!/bin/bash
clear
echo "Get MQTT Sessions"
echo

source ./config.sh.include

# /msgVpns/{msgVpnName}/mqttSessions

curl -X GET \
  -u $SEMPV2_USER:$SEMPV2_PASSWORD \
  $SEMPV2_BASEPATH/msgVpns/$MESSAGE_VPN/mqttSessions

echo
echo
# The End.
