#!/usr/bin/env bash
# SmartPark — register the device with FIWARE. Run once, after "docker compose up -d".
# Usage:  bash provision.sh
set -e
IOTA=http://localhost:4041
ORION=http://localhost:1026
H_SERVICE="fiware-service: smartpark"
H_PATH="fiware-servicepath: /"
H_JSON="Content-Type: application/json"
HERE="$(cd "$(dirname "$0")" && pwd)"

echo "1/3  Service group..."
curl -sS -X POST "$IOTA/iot/services" -H "$H_SERVICE" -H "$H_PATH" -H "$H_JSON" \
     -d @"$HERE/01_service_group.json" -w "  HTTP %{http_code}\n"

echo "2/3  Device parking001..."
curl -sS -X POST "$IOTA/iot/devices" -H "$H_SERVICE" -H "$H_PATH" -H "$H_JSON" \
     -d @"$HERE/02_device.json" -w "  HTTP %{http_code}\n"

echo "3/3  History subscription..."
curl -sS -X POST "$ORION/v2/subscriptions" -H "$H_SERVICE" -H "$H_PATH" -H "$H_JSON" \
     -d @"$HERE/03_subscription.json" -w "  HTTP %{http_code}\n"

echo
echo "Expected: 201 for each. 409 on the first two means it was already registered - that is fine."
