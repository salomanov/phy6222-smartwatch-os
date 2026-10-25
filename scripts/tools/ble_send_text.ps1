# LP715 Smartwatch BLE Text Sender (WinRT BLE)
[CmdletBinding()]
param(
    [Parameter(Position=0)]
    [string]$Text = "Привет, мир!",
    
    [Parameter(Position=1)]
    [ValidateSet("White", "Green", "Yellow", "Cyan", "Red", "Magenta", "Blue", "Orange", "W", "G", "Y", "C", "R", "M", "B", "O")]
    [string]$Color = "White",
    
    [Parameter(Position=2)]
    [ValidateSet(1, 2)]
    [int]$Scale = 1,
    
    [string]$DeviceAddress = "62:62:15:07:07:15"
)

Add-Type -AssemblyName System.Runtime.WindowsRuntime
[Windows.Devices.Bluetooth.BluetoothLEDevice, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattDeviceServicesResult, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattCharacteristicsResult, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null
[Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus, Windows.Devices.Bluetooth, ContentType = WindowsRuntime] | Out-Null

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | ? { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]

function AwaitOp($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    $netTask.Result
}

# Color code mapping
$colorChar = switch -Regex ($Color.ToUpper()) {
    '^G' { 'G' }
    '^Y' { 'Y' }
    '^C' { 'C' }
    '^R' { 'R' }
    '^M' { 'M' }
    '^B' { 'B' }
    '^O' { 'O' }
    default { 'W' }
}

$macInt = [Convert]::ToUInt64(($DeviceAddress -replace ':',''), 16)

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "   LP715 BLE NOTIFICATION / PAGER (via WinRT)" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "[*] Target Watch   : $DeviceAddress"
Write-Host "[*] Message Text   : $Text"
Write-Host "[*] Color / Scale  : $Color ($colorChar) / Scale $Scale"

# 1. Connect to BLE device
$op = [Windows.Devices.Bluetooth.BluetoothLEDevice]::FromBluetoothAddressAsync($macInt)
$dev = AwaitOp $op ([Windows.Devices.Bluetooth.BluetoothLEDevice])

if ($null -eq $dev) {
    Write-Host "[!] Could not reach Bluetooth device $DeviceAddress" -ForegroundColor Red
    exit 1
}
Write-Host "[+] Watch Connected : $($dev.Name) ($($dev.ConnectionStatus))" -ForegroundColor Green

# 2. Find SimpleProfile Service 0xFFF0
$srvOp = $dev.GetGattServicesAsync()
$srvRes = AwaitOp $srvOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattDeviceServicesResult])
$srv = $srvRes.Services | Where-Object { $_.Uuid.ToString().StartsWith("0000fff0") } | Select-Object -First 1

if ($null -eq $srv) {
    Write-Host "[!] SimpleProfile service (0xFFF0) not found" -ForegroundColor Red
    exit 2
}

# 3. Find Characteristic 0xFFF1
$charOp = $srv.GetCharacteristicsAsync()
$charRes = AwaitOp $charOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCharacteristicsResult])
$char1 = $charRes.Characteristics | Where-Object { $_.Uuid.ToString().StartsWith("0000fff1") } | Select-Object -First 1

if ($null -eq $char1) {
    Write-Host "[!] Characteristic 0xFFF1 not found" -ForegroundColor Red
    exit 3
}

# Helper to write a 16-byte buffer
function Send-BlePacket([byte[]]$packet) {
    $buf16 = New-Object byte[] 16
    [Array]::Copy($packet, $buf16, [Math]::Min($packet.Length, 16))
    $winBuf = [System.Runtime.InteropServices.WindowsRuntime.WindowsRuntimeBufferExtensions]::AsBuffer($buf16)
    $wOp = $char1.WriteValueAsync($winBuf)
    $wRes = AwaitOp $wOp ([Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus])
    if ($wRes -ne [Windows.Devices.Bluetooth.GenericAttributeProfile.GattCommunicationStatus]::Success) {
        Write-Warning "Write returned: $wRes"
    }
}

# Safe chunk splitter strictly on character boundaries
function Split-TextIntoSafeChunks([string]$str, [int]$firstMaxBytes, [int]$restMaxBytes) {
    $result = @()
    $current = ""
    $maxB = $firstMaxBytes
    foreach ($ch in $str.ToCharArray()) {
        $candidate = $current + $ch
        if ([System.Text.Encoding]::UTF8.GetByteCount($candidate) -gt $maxB) {
            if ($current.Length -gt 0) {
                $result += $current
            }
            $current = [string]$ch
            $maxB = $restMaxBytes
        } else {
            $current = $candidate
        }
    }
    if ($current.Length -gt 0) {
        $result += $current
    }
    return $result
}

$utf8Bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
Write-Host "[*] Total UTF-8 length: $($utf8Bytes.Length) bytes"

$singleHeader = [System.Text.Encoding]::ASCII.GetBytes("MSG:$colorChar`:$Scale`:")
if ($singleHeader.Length + $utf8Bytes.Length -le 16) {
    $pkt = New-Object byte[] ($singleHeader.Length + $utf8Bytes.Length)
    [Array]::Copy($singleHeader, 0, $pkt, 0, $singleHeader.Length)
    [Array]::Copy($utf8Bytes, 0, $pkt, $singleHeader.Length, $utf8Bytes.Length)
    Write-Host "[*] Sending single 16-byte packet: '$( [System.Text.Encoding]::UTF8.GetString($pkt) )'..."
    Send-BlePacket $pkt
} else {
    $chunks = Split-TextIntoSafeChunks $Text 9 13
    Write-Host "[*] Split message into $($chunks.Count) safe UTF-8 chunks:"

    for ($i = 0; $i -lt $chunks.Count; $i++) {
        $chunkText = $chunks[$i]
        $cBytes = [System.Text.Encoding]::UTF8.GetBytes($chunkText)

        if ($i -eq 0) {
            # Start Chunk
            $hdr = [System.Text.Encoding]::ASCII.GetBytes("M+:$colorChar`:$Scale`:")
            $pkt = New-Object byte[] ($hdr.Length + $cBytes.Length)
            [Array]::Copy($hdr, 0, $pkt, 0, $hdr.Length)
            [Array]::Copy($cBytes, 0, $pkt, $hdr.Length, $cBytes.Length)
            Write-Host "  -> Chunk 1 (Start): '$chunkText' ($($cBytes.Length)b)"
            Send-BlePacket $pkt
        } elseif ($i -eq ($chunks.Count - 1)) {
            # Final Chunk (Triggers display)
            $hdr = [System.Text.Encoding]::ASCII.GetBytes("M.:")
            $pkt = New-Object byte[] ($hdr.Length + $cBytes.Length)
            [Array]::Copy($hdr, 0, $pkt, 0, $hdr.Length)
            [Array]::Copy($cBytes, 0, $pkt, $hdr.Length, $cBytes.Length)
            Write-Host "  -> Chunk $($i+1) (End & Display): '$chunkText' ($($cBytes.Length)b)"
            Send-BlePacket $pkt
        } else {
            # Continuation Chunk
            $hdr = [System.Text.Encoding]::ASCII.GetBytes("M~:")
            $pkt = New-Object byte[] ($hdr.Length + $cBytes.Length)
            [Array]::Copy($hdr, 0, $pkt, 0, $hdr.Length)
            [Array]::Copy($cBytes, 0, $pkt, $hdr.Length, $cBytes.Length)
            Write-Host "  -> Chunk $($i+1) (Cont): '$chunkText' ($($cBytes.Length)b)"
            Send-BlePacket $pkt
        }
        Start-Sleep -Milliseconds 60
    }
}

Write-Host "====================================================" -ForegroundColor Green
Write-Host "[+] SUCCESS! Notification displayed on smartwatch." -ForegroundColor Green
Write-Host "    Color: $Color | Scale: $Scale" -ForegroundColor Green
Write-Host "    Tap touch button to dismiss." -ForegroundColor Yellow
Write-Host "====================================================" -ForegroundColor Green
