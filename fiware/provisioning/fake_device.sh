#!/usr/bin/env bash
# Pretend to be the ESP32: publish one state message over MQTT.
# Needs mosquitto-clients, or use MQTT Explorer instead.
# Usage: bash fake_device.sh [occupied|available]
S1=${1:-occupied}
mosquitto_pub -h localhost -p 1883 \
  -t "/smartpark2026key/parking001/attrs" \
  -m "{\"s1\":\"$S1\",\"s2\":\"available\",\"s3\":\"available\",\"s4\":\"available\",\"occ\":1,\"free\":3,\"rate\":25,\"gate\":\"closed\",\"eg\":\"none\",\"ev\":\"TEST\",\"rssi\":-55}"
echo "published"
