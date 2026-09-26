param(
    [string]$Port = 'COM14',
    [string]$DeployRoot = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
$selected = @(
    'main.py',
    'stm32_link.py',
    'data_touch/__init__.py',
    'data_touch/app.py',
    'data_touch/camera.py',
    'data_touch/config.py',
    'data_touch/key.py',
    'data_touch/result_log.py',
    'texture_structure_tensor_v1/config.py',
    'texture_structure_tensor_v1/multivariate_gunay_port.py',
    'texture_structure_tensor_v1/multivariate_gunay_reference.py',
    'texture_structure_tensor_v1/st_utils.py',
    'texture_structure_tensor_v1/structure_tensor_gunay.py'
)

function Receive-Ack([System.IO.Ports.SerialPort]$Serial, [int]$TimeoutMs = 10000) {
    $deadline = [Environment]::TickCount64 + $TimeoutMs
    $all = ''
    while ([Environment]::TickCount64 -lt $deadline) {
        Start-Sleep -Milliseconds 25
        $chunk = $Serial.ReadExisting()
        if ($chunk) {
            $all += $chunk
            if ($all -match '__K230_DEPLOY_ACK__') { return $all }
        }
    }
    throw "Timed out waiting for board acknowledgement: $all"
}

function Send-Command([System.IO.Ports.SerialPort]$Serial, [string]$Command, [int]$TimeoutMs = 10000) {
    $Serial.DiscardInBuffer()
    # Build the marker at runtime so the REPL command echo cannot look like an ACK.
    $Serial.Write("`r`n$Command;print('__K230_'+'DEPLOY_ACK__')`r`n")
    return Receive-Ack $Serial $TimeoutMs
}

$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 5000
$serial.DtrEnable = $true
$serial.RtsEnable = $true
try {
    $serial.Open()
    Start-Sleep -Milliseconds 300
    1..3 | ForEach-Object { $serial.Write([byte[]](3),0,1); Start-Sleep -Milliseconds 250 }
    $identity = Send-Command $serial "import sys,os,gunay_native;print('K230_DEPLOY_ID',sys.implementation,sys.platform,hasattr(gunay_native,'region_stats_fused_span_query'))"
    if ($identity -notmatch 'k230_canmv_01studio' -or $identity -notmatch 'True') {
        throw "Positive K230/final-native identity failed: $identity"
    }
    $hostNow = Get-Date
    $weekday = ([int]$hostNow.DayOfWeek + 6) % 7
    $rtcTuple = "({0},{1},{2},{3},{4},{5},{6},0)" -f $hostNow.Year,$hostNow.Month,$hostNow.Day,$weekday,$hostNow.Hour,$hostNow.Minute,$hostNow.Second
    $rtcReply = Send-Command $serial "import machine,time;machine.RTC().datetime($rtcTuple);print('K230_RTC_SET',time.localtime())"
    if ($rtcReply -notmatch [regex]::Escape("K230_RTC_SET ($($hostNow.Year), $($hostNow.Month), $($hostNow.Day),")) {
        throw "RTC initialization failed: $rtcReply"
    }
    $mkdirCode = "try:`n os.mkdir('/sdcard/data_touch')`nexcept OSError:`n pass`ntry:`n os.mkdir('/sdcard/data_touch_results')`nexcept OSError:`n pass`ntry:`n os.mkdir('/sdcard/texture_structure_tensor_v1')`nexcept OSError:`n pass"
    $mkdirB64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($mkdirCode))
    $null = Send-Command $serial "import os;exec(__import__('ubinascii').a2b_base64('$mkdirB64'))"
    foreach ($relative in $selected) {
        $hostPath = Join-Path $DeployRoot ($relative.Replace('/','\'))
        if (-not (Test-Path -LiteralPath $hostPath -PathType Leaf)) { throw "Missing deploy file: $hostPath" }
        $target = '/sdcard/' + $relative
        $null = Send-Command $serial "f=open('$target','wb')"
        $bytes = [System.IO.File]::ReadAllBytes($hostPath)
        for ($offset = 0; $offset -lt $bytes.Length; $offset += 512) {
            $count = [Math]::Min(512, $bytes.Length - $offset)
            $part = New-Object byte[] $count
            [Array]::Copy($bytes, $offset, $part, 0, $count)
            $b64 = [Convert]::ToBase64String($part)
            $serial.DiscardInBuffer()
            $serial.Write("f.write(__import__('ubinascii').a2b_base64('$b64'));print('__K230_'+'DEPLOY_ACK__')`r`n")
            $null = Receive-Ack $serial 10000
        }
        $reply = Send-Command $serial "f.close();print('K230_DEPLOY_FILE','$relative',os.stat('$target')[6])" 30000
        if ($reply -notmatch [regex]::Escape("K230_DEPLOY_FILE $relative $($bytes.Length)")) {
            throw "Device size mismatch for $relative : $reply"
        }
        Write-Output "UPLOAD_PASS $relative bytes=$($bytes.Length)"
    }
    Write-Output 'K230_FINAL_DEPLOY_UPLOAD=PASS'
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
