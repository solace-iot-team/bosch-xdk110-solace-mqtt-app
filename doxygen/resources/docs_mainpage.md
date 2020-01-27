# Bosch XDK110 Solace MQTT App  {#mainpage}

## Intro
A 'reference mqtt client app' for the Bosch XDK110 to demonstrate how a device can be programmed to interact with a wider IoT platform.

The main features are:
- Configuration API - allows for a device management system to remotely configure the operations of the app
- Command API - allows to send commands to the app
- Status API - sends regular and ad-hoc status information
- Sampling & Telemetry API - sends configurable sensor readings at regular intervals
- Buttons API - sends a message when pressing / releasing a button

## General
* XDK version: $(XDK_VERSION)
* Solace App Version: $(SOLACE_APP_VERSION)
* Author: $(SOLACE_APP_AUTHOR)
* Date: $(SOLACE_APP_DATE)

## The Software Architecture


@image html solace-app-sw-architecture.png width=800


----
The End.
