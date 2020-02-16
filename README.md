# satel2mqtt
Utility allows Satel CA-10 (and probably other older models) alarm system input events to publish to MQTT topic.
Originally designed to integrate old alarm with Domoticz trough MQTT messaging protocol, 
but it is very flexible and can be easily ajusted to publish to any MQTT topic with required message.

  Usage: satel2mqtt [-h] [-v] [-d] -H MQTT_HOST [-p MQTT_PORT] [-D tty_device] [-t MQTT_TOPIC]
  
  -h : print help and usage\
  -v : enable debug messages\
  -d : fork to background\
  -H : mqtt host to connect to. Defaults to localhost.\
  -p : network port to connect to. Defaults to 1883.\
  -D : tty device to listen to Satel logs. Defaults to /dev/ttyUSB0\
  -t : mqtt topic to publish to. Defaults to 'domoticz/in'\


Required configuration on Satel CA10 -
Connected RS-232 port, enabled printing event log in central configuration.
More details on how to connect and configure system is available on official documentation
https://www.satel.pl/pl/download/instrukcje/ca10_ii_en_0509.pdf
Enable 'silent monitoring' on one of the zones, which does not cause alarm signal.
