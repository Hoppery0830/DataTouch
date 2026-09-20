param([string]$Port = 'COM14', [int]$TimeoutSeconds = 30)
$ErrorActionPreference = 'Stop'
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.DtrEnable = $true; $serial.RtsEnable = $true
try {
    $serial.Open(); Start-Sleep -Milliseconds 250; $serial.DiscardInBuffer()
    $serial.Write([byte[]](4),0,1)
    $deadline = [Environment]::TickCount64 + ([int64]$TimeoutSeconds * 1000)
    $all = ''
    while ([Environment]::TickCount64 -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $chunk = $serial.ReadExisting()
        if ($chunk) {
            $all += $chunk
            if ($all -match 'K230_DEPLOY_READY') { break }
            if ($all -match 'Traceback \(most recent call last\)') { break }
        }
    }
    Write-Output $all
    if ($all -notmatch 'K230_DEPLOY_READY') { throw 'Final deployment did not enter idle state' }
    Write-Output 'K230_FINAL_DEPLOY_IDLE=PASS'
}
finally { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
