#!/usr/bin/env bash
# SmartPark - quick health check of the FIWARE stack.  Usage: bash check.sh
H="-H fiware-service:smartpark -H fiware-servicepath:/"
echo "== containers =="
docker compose -f "$(dirname "$0")/../docker-compose.yml" ps
echo
echo "== Orion version =="       ; curl -sS http://localhost:1026/version | head -5
echo "== IoT Agent =="           ; curl -sS http://localhost:4041/iot/about
echo
echo "== registered device =="   ; curl -sS $H http://localhost:4041/iot/devices
echo
echo "== current entity =="      ; curl -sS $H "http://localhost:1026/v2/entities?options=keyValues"
echo
echo "== subscriptions =="       ; curl -sS $H http://localhost:1026/v2/subscriptions
echo
echo "== history rows (QuantumLeap) =="
curl -sS $H "http://localhost:8668/v2/entities/urn:ngsi-ld:ParkingSite:001/attrs/occupiedCount?lastN=5"
