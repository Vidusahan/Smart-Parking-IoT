# SmartPark - register the device with FIWARE (Windows PowerShell).
# Run once, after "docker compose up -d":   .\provision.ps1
$iota  = "http://localhost:4041"
$orion = "http://localhost:1026"
$headers = @{ "fiware-service" = "smartpark"; "fiware-servicepath" = "/" }
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Send-Json($url, $file) {
  $body = Get-Content -Raw -Path (Join-Path $here $file)
  try {
    Invoke-RestMethod -Method Post -Uri $url -Headers $headers -ContentType "application/json" -Body $body | Out-Null
    Write-Host "  OK  $file"
  } catch {
    Write-Host "  FAILED  $file  ->  $($_.Exception.Message)"
  }
}

Write-Host "1/3  Service group..."
Send-Json "$iota/iot/services" "01_service_group.json"
Write-Host "2/3  Device parking001..."
Send-Json "$iota/iot/devices" "02_device.json"
Write-Host "3/3  History subscription..."
Send-Json "$orion/v2/subscriptions" "03_subscription.json"
Write-Host ""
Write-Host "A 'Conflict/409' on the first two means it was already registered - that is fine."
