param([string]$Port = 'COM14')
$ErrorActionPreference = 'Stop'
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 5000
$serial.DtrEnable = $true
$serial.RtsEnable = $true
try {
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()
    $readCode = @'
import gc
print('K230_FINAL_LOG_BEGIN')
_count = 0
_last_complete = True
try:
    _handle = open('/sdcard/data_touch_results/u_curve.txt', 'r')
except OSError as _error:
    if _error.args[0] != 2:
        raise
    _handle = None
if _handle is not None:
    try:
        while True:
            _line = _handle.readline(512)
            if not _line:
                break
            print(_line, end='')
            _last_complete = _line.endswith('\n')
            if _last_complete:
                _count += 1
    finally:
        _handle.close()
if not _last_complete:
    _count += 1
    print()
print('K230_FINAL_LOG_END', _count, gc.mem_free())
'@
    $readEncoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($readCode))
    $serial.Write("`r`nexec(__import__('ubinascii').a2b_base64('$readEncoded'))`r`n")
    $deadline = [Environment]::TickCount64 + 10000
    $all = ''
    while ([Environment]::TickCount64 -lt $deadline) {
        Start-Sleep -Milliseconds 50
        $chunk = $serial.ReadExisting()
        if ($chunk) {
            $all += $chunk
            $deadline = [Environment]::TickCount64 + 10000
        }
        if ($all -match 'K230_FINAL_LOG_END \d+ ') { break }
    }
    if ($all -notmatch 'K230_FINAL_LOG_END \d+ ') { throw "No final log marker: $all" }
    Write-Output $all
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
