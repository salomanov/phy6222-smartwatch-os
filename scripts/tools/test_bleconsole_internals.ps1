[Windows.Storage.Streams.ByteOrder, Windows.Storage.Streams, ContentType = WindowsRuntime] | Out-Null
$asm = [Reflection.Assembly]::LoadFile((Resolve-Path .\tools\ble_console\BLEConsole.exe))
$df = $asm.GetType('BLEConsole.Utils.DataFormatter')
$bo = [Windows.Storage.Streams.ByteOrder]::LittleEndian
$m_format = $df.GetMethod('FormatData', [Reflection.BindingFlags]'Static,NonPublic,Public')
$m_read = $df.GetMethod('ReadBufferToBytes', [Reflection.BindingFlags]'Static,NonPublic,Public')

$tests = @(
    @("Hex", "00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF"),
    @("Hex", "0x00 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF"),
    @("Hex", "54494D453A3230"),
    @("ASCII", "TIME:20:25:00___"),
    @("UTF8", "TIME:20:25:00___")
)

foreach ($t in $tests) {
    $fName = $t[0]
    $val = $t[1]
    $fmt = [Enum]::Parse($asm.GetType('BLEConsole.Enums.DataFormat'), $fName)
    try {
        $buf = $m_format.Invoke($null, @($val, $fmt, $bo))
        if ($buf -ne $null) {
            $bytes = $m_read.Invoke($null, @($buf))
            Write-Host "$($fName) '$val' -> Length: $($bytes.Length), Bytes: $([BitConverter]::ToString($bytes))"
        } else {
            Write-Host "$($fName) '$val' -> Buffer was null"
        }
    } catch {
        Write-Host "$($fName) '$val' -> Error: $($_.Exception.InnerException.Message)"
    }
}
