
#!/bin/bash

# Node Red uses these at start-up
export MQTT_BROKER_HOST="mr1dns3dpz5u9l.messaging.solace.cloud"
export MQTT_BROKER_PORT="1883"
export MQTT_BROKER_USER="solace-cloud-client"
export MQTT_BROKER_PASSWORD="bppg33clnljgqujajvqodor1vs"

node-red -s solace-xdk110-mgmt-settings.js


# The End.
