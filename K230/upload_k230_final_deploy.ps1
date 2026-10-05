param(
    [string]$Port = 'COM14',
    [string]$DeployRoot = $PSScriptRoot,
    [string]$ResultBackupPath = ''
)

$ErrorActionPreference = 'Stop'
if ($ResultBackupPath -and -not (Test-Path -LiteralPath $ResultBackupPath -PathType Leaf)) {
    throw "Missing result backup: $ResultBackupPath"
}
$selected = @(
    'main.py',
    'production_main.py',
    'debug_main.py',
    'mode_config.py',
    'set_debug_mode.py',
    'set_production_mode.py',
    'stm32_link.py',
    'data_touch/__init__.py',
    'data_touch/app.py',
    'data_touch/camera.py',
    'data_touch/config.py',
    'data_touch/key.py',
    'data_touch/result_log.py'
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
    $mkdirCode = "try:`n os.mkdir('/sdcard/data_touch')`nexcept OSError:`n pass`ntry:`n os.mkdir('/sdcard/data_touch_results')`nexcept OSError:`n pass"
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
    if ($ResultBackupPath) {
        if (-not (Test-Path -LiteralPath $ResultBackupPath -PathType Leaf)) {
            throw "Missing result backup: $ResultBackupPath"
        }
        $target = '/sdcard/data_touch_results/u_curve.txt.restore_pending'
        $null = Send-Command $serial "f=open('$target','wb')"
        $bytes = [System.IO.File]::ReadAllBytes($ResultBackupPath)
        for ($offset = 0; $offset -lt $bytes.Length; $offset += 512) {
            $count = [Math]::Min(512, $bytes.Length - $offset)
            $part = New-Object byte[] $count
            [Array]::Copy($bytes, $offset, $part, 0, $count)
            $b64 = [Convert]::ToBase64String($part)
            $serial.DiscardInBuffer()
            $serial.Write("f.write(__import__('ubinascii').a2b_base64('$b64'));print('__K230_'+'DEPLOY_ACK__')`r`n")
            $null = Receive-Ack $serial 10000
        }
        $reply = Send-Command $serial "f.flush();f.close();print('K230_RESULT_RESTORED',os.stat('$target')[6])" 30000
        if ($reply -notmatch [regex]::Escape("K230_RESULT_RESTORED $($bytes.Length)")) {
            throw "Result restore size mismatch: $reply"
        }
        $restoreCode = @'
import os
_result_path = '/sdcard/data_touch_results/u_curve.txt'
_staged_path = _result_path + '.restore_pending'
try:
    _old_size = os.stat(_result_path)[6]
except OSError as _error:
    if _error.args[0] != 2:
        raise
    _old_size = None
if _old_size is not None:
    _backup_path = _result_path + '.before_restore'
    _backup_number = 1
    while True:
        try:
            os.stat(_backup_path)
        except OSError as _error:
            if _error.args[0] == 2:
                break
            raise
        _backup_path = _result_path + '.before_restore.%d' % _backup_number
        _backup_number += 1
    with open(_result_path, 'rb') as _reader, open(_backup_path, 'wb') as _writer:
        while True:
            _chunk = _reader.read(1024)
            if not _chunk:
                break
            if _writer.write(_chunk) != len(_chunk):
                raise OSError('short result backup write')
        _writer.flush()
    print('K230_RESULT_BACKUP', _backup_path, _old_size)
_previous_path = _result_path + '.replace_previous'
if _old_size is not None:
    os.rename(_result_path, _previous_path)
try:
    os.rename(_staged_path, _result_path)
except Exception:
    if _old_size is not None:
        os.rename(_previous_path, _result_path)
    raise
if _old_size is not None:
    os.remove(_previous_path)
print('K230_RESULT_RESTORE_COMMITTED', os.stat(_result_path)[6])
'@
        $restoreEncoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($restoreCode))
        $reply = Send-Command $serial "exec(__import__('ubinascii').a2b_base64('$restoreEncoded'))" 30000
        if ($reply -notmatch [regex]::Escape("K230_RESULT_RESTORE_COMMITTED $($bytes.Length)")) {
            throw "Result restore commit failed: $reply"
        }
        Write-Output "RESTORE_PASS data_touch_results/u_curve.txt bytes=$($bytes.Length)"
    }
    Write-Output 'K230_DUAL_MODE_UPLOAD=PASS'
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
