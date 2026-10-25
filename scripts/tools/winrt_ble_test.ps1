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

Write-Host "[*] Looking up device 0x$($macInt.ToString('X12'))..."
$op = [Windows.Devices.Bluetooth.BluetoothLEDevice]::FromBluetoothAddressAsync($macInt)
$dev = AwaitOp $op ([Windows.Devices.Bluetooth.BluetoothLEDevice])

Write-Host "[+] Device: $($dev.Name), Connected: $($dev.ConnectionStatus)"

$srvOp = $dev.GetGattServicesAsync()
$srvRes = AwaitOp $srvOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattDeviceServicesResult])
$srv = $srvRes.Services | Where-Object { $_.Uuid.ToString().StartsWith("0000fff0") } | Select-Object -First 1

$charOp = $srv.GetCharacteristicsAsync()
$charRes = AwaitOp $charOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCharacteristicsResult])
$char1 = $charRes.Characteristics | Where-Object { $_.Uuid.ToString().StartsWith("0000fff1") } | Select-Object -First 1

# 1. Read before
$rOp = $char1.ReadValueAsync()
$rRes = AwaitOp $rOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattReadResult])
$bytesBefore = $m_read.Invoke($null, @($rRes.Value))
Write-Host "[*] Value BEFORE write: $([BitConverter]::ToString($bytesBefore))"

# 2. Prepare 16 bytes: "TIME:20:41:00\0\0\0"
$rawBytes = [System.Text.Encoding]::ASCII.GetBytes("TIME:20:41:00`0`0`0")
Write-Host "[*] Sending $($rawBytes.Length) bytes: $([BitConverter]::ToString($rawBytes))"
$buf = [System.Runtime.InteropServices.WindowsRuntime.WindowsRuntimeBufferExtensions]::AsBuffer($rawBytes)

# 3. Write
$wOp = $char1.WriteValueAsync($buf)
$wStat = AwaitOp $wOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus])
Write-Host "[+] Write CommunicationStatus: $wStat"

# 4. Read after
$rOp2 = $char1.ReadValueAsync()
$rRes2 = AwaitOp $rOp2 ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattReadResult])
$bytesAfter = $m_read.Invoke($null, @($rRes2.Value))
Write-Host "[+] Value AFTER write: $([BitConverter]::ToString($bytesAfter))"
Write-Host "[+] ASCII decoded: '$([System.Text.Encoding]::ASCII.GetString($bytesAfter))'"
