param(
    [string]$Port = 'COM14',
    [int]$Measurements = 10,
    [int]$TimeoutSeconds = 1200,
    [string]$CapturePath = (Join-Path $PSScriptRoot 'artifacts\device_serial_capture.txt')
)

$ErrorActionPreference = 'Stop'
$directory = Split-Path -Parent $CapturePath
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 5000
$serial.DtrEnable = $true
$serial.RtsEnable = $true
try {
    $serial.Open()
    Start-Sleep -Milliseconds 300
    1..3 | ForEach-Object { $serial.Write([byte[]](3),0,1); Start-Sleep -Milliseconds 250 }
    $serial.DiscardInBuffer()
    $command = "import sys;sys.path.insert(0,'/sdcard');from data_touch.app import main;_deploy_rows=main($Measurements);print('K230_DEPLOY_ROWS',repr(_deploy_rows))"
    $serial.Write("`r`n$command`r`n")
    $deadline = [Environment]::TickCount64 + ([int64]$TimeoutSeconds * 1000)
    $all = ''
    $done = $false
    while ([Environment]::TickCount64 -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $chunk = $serial.ReadExisting()
        if ($chunk) {
            $all += $chunk
            Write-Output $chunk
            if ($all -match [regex]::Escape("K230_DEPLOY_ACCEPTANCE_DONE count=$Measurements")) { $done = $true; break }
            if ($all -match 'Traceback \(most recent call last\)') { break }
        }
    }
    [System.IO.File]::WriteAllText($CapturePath, $all, [System.Text.UTF8Encoding]::new($false))
    if (-not $done) { throw "Acceptance did not complete. Capture: $CapturePath" }
    Write-Output 'K230_FINAL_DEPLOY_DEVICE_ACCEPTANCE=COMPLETED'
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
