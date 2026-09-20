param([string]$Port = 'COM14')
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.DtrEnable = $true; $serial.RtsEnable = $true
try {
    $serial.Open(); Start-Sleep -Milliseconds 250; $serial.DiscardInBuffer()
    $serial.Write("`r`nimport machine,time;_r=machine.RTC();print('K230_RTC_PROBE',time.localtime(),_r.datetime(),_r.now());help(_r.datetime)`r`n")
    Start-Sleep -Milliseconds 1200
    Write-Output $serial.ReadExisting()
}
finally { if ($serial.IsOpen) { $serial.Close() }; $serial.Dispose() }
