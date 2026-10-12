#!/usr/bin/env bash
# Send a gate command through Orion.   Usage: bash send_command.sh gate open
ATTR=${1:-gate}
VALUE=${2:-open}
curl -sS -X PATCH "http://localhost:1026/v2/entities/urn:ngsi-ld:ParkingSite:001/attrs" \
  -H "fiware-service: smartpark" -H "fiware-servicepath: /" -H "Content-Type: application/json" \
  -d "{\"$ATTR\": {\"type\": \"command\", \"value\": \"$VALUE\"}}" -w "HTTP %{http_code}\n"
