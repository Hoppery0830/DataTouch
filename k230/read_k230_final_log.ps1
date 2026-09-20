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
    $serial.Write("`r`nimport gc;_p='/sdcard/data_touch_results/u_curve.txt';_h=open(_p,'r');_t=_h.read();_h.close();print('K230_FINAL_LOG_BEGIN');print(_t,end='');print('K230_FINAL_LOG_END',len(_t.splitlines()),gc.mem_free())`r`n")
    $deadline = [Environment]::TickCount64 + 10000
    $all = ''
    while ([Environment]::TickCount64 -lt $deadline) {
        Start-Sleep -Milliseconds 50
        $all += $serial.ReadExisting()
        if ($all -match 'K230_FINAL_LOG_END') { break }
    }
    if ($all -notmatch 'K230_FINAL_LOG_END') { throw "No final log marker: $all" }
    Write-Output $all
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
