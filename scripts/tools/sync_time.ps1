# Sync local PC time to LP715 Smartwatch via BLE WinRT
[CmdletBinding()]
param()

Add-Type -AssemblyName System.Runtime.WindowsRuntime
[Windows.Devices.Bluetooth.BluetoothLEDevice, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattDeviceServicesResult, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattCharacteristicsResult, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattReadResult, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null

$asm = [Reflection.Assembly]::LoadFile((Resolve-Path .\tools\ble_console\BLEConsole.exe))
$df = $asm.GetType('BLEConsole.Utils.DataFormatter')
$m_read = $df.GetMethod('ReadBufferToBytes', [Reflection.BindingFlags]'Static,NonPublic,Public')

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | ? { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]

function AwaitOp($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    $netTask.Result
}

$macStr = "62:62:15:07:07:15"
$macInt = [Convert]::ToUInt64(($macStr -replace ':',''), 16)
$now = Get-Date

Write-Host "===================================================="
Write-Host "   LP715 BLE TIME SYNCHRONIZER (via BLE WinRT)"
Write-Host "===================================================="
Write-Host "[*] Current PC Time : $($now.ToString('HH:mm:ss'))"

$op = [Windows.Devices.Bluetooth.BluetoothLEDevice]::FromBluetoothAddressAsync($macInt)
$dev = AwaitOp $op ([Windows.Devices.Bluetooth.BluetoothLEDevice])

if ($null -eq $dev) {
    Write-Host "[!] Could not reach Bluetooth device $macStr" -ForegroundColor Red
    exit 1
}

Write-Host "[+] Watch Connected : $($dev.Name) ($($dev.ConnectionStatus))"

$srvOp = $dev.GetGattServicesAsync()
$srvRes = AwaitOp $srvOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattDeviceServicesResult])
$srv = $srvRes.Services | Where-Object { $_.Uuid.ToString().StartsWith("0000fff0") } | Select-Object -First 1

if ($null -eq $srv) {
    Write-Host "[!] Service 0xFFF0 not found" -ForegroundColor Red
    exit 2
}

$charOp = $srv.GetCharacteristicsAsync()
$charRes = AwaitOp $charOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCharacteristicsResult])
$char1 = $charRes.Characteristics | Where-Object { $_.Uuid.ToString().StartsWith("0000fff1") } | Select-Object -First 1

if ($null -eq $char1) {
    Write-Host "[!] Characteristic 0xFFF1 not found" -ForegroundColor Red
    exit 3
}

# Payload: exactly 16 bytes "TIME:HH:MM:SS\0\0\0"
$timeStr = "TIME:" + $now.ToString("HH:mm:ss") + "`0`0`0"
$rawBytes = [System.Text.Encoding]::ASCII.GetBytes($timeStr)
$buf = [System.Runtime.InteropServices.WindowsRuntime.WindowsRuntimeBufferExtensions]::AsBuffer($rawBytes)

Write-Host "[*] Writing 16-byte packet : '$($timeStr.Trim("`0"))'..."
$wOp = $char1.WriteValueAsync($buf)
$wStat = AwaitOp $wOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus])

if ($wStat -eq [Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus]::Success) {
    Write-Host "[+] Status: SUCCESS! Time synchronized." -ForegroundColor Green
} else {
    Write-Host "[!] Write failed: $wStat" -ForegroundColor Red
    exit 4
}

# Verify readback
$rOp = $char1.ReadValueAsync()
$rRes = AwaitOp $rOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattReadResult])
$bytesAfter = $m_read.Invoke($null, @($rRes.Value))
$readStr = [System.Text.Encoding]::ASCII.GetString($bytesAfter).Trim("`0")
Write-Host "[+] Verified Readback : '$readStr' (Hex: $([BitConverter]::ToString($bytesAfter)))"
Write-Host "===================================================="
