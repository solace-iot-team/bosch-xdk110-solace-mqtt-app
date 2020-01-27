# Release Notes

## Release 2.0.0

First release based on prototype developed Jan-May 2019.

### Known Issues

#### QoS = 1
  - there seems to be an issue with sending QoS=1 messages
  - unsure if this is an issue with the Serval Stack or wrong use of the API
  - XDK disconnects very frequently when sending QoS=1 messages

#### Hardfault ...
- in certain circumstances, the app creates a hard fault
- suspected cause:
  - some static variables are not protected and could be read/written simultaneously from different threads

#### MQTT Configuration
- changing the mqtt configuration via the API is not supported / not implemented
- this should be implemented in conjunction with a new bootstrap concept

#### Release Build
- does not compile, assert() undefined


----
The End.
