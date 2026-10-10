param(
    [string]$Port = 'COM7',
    [int]$Blocks = 100
)

$ErrorActionPreference = 'Stop'

if ($Blocks -lt 1) {
    throw 'Blocks must be at least 1.'
}

$uartPort = [System.IO.Ports.SerialPort]::new(
    $Port, 115200,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)

$uartPort.Handshake = [System.IO.Ports.Handshake]::None
$uartPort.ReadTimeout = 3000
$uartPort.WriteTimeout = 3000

# Fixed seed makes the test data reproducible.
$uartRandom = [System.Random]::new(12345)

try {
    $uartPort.Open()
    $uartPort.DiscardInBuffer()

    for ($block = 1; $block -le $Blocks; $block++) {
        $sent = [byte[]]::new(32)
        $received = [byte[]]::new(32)

        # Binary data, including possible zero and non-ASCII bytes.
        $uartRandom.NextBytes($sent)

        $uartPort.Write($sent, 0, $sent.Length)

        $count = 0
        while ($count -lt $received.Length) {
            $count += $uartPort.Read(
                $received,
                $count,
                $received.Length - $count
            )
        }

        if ([System.BitConverter]::ToString($sent) -ne
            [System.BitConverter]::ToString($received)) {
            throw "Byte mismatch in block $block."
        }

        if (($block % 10 -eq 0) -or ($block -eq $Blocks)) {
            Write-Host "Verified $block / $Blocks blocks."
        }
    }

    Write-Host (
        "PASS: {0} blocks, {1} bytes matched." -f
        $Blocks, ($Blocks * 32)
    ) -ForegroundColor Green
}
finally {
    if ($uartPort.IsOpen) {
        $uartPort.Close()
    }
    $uartPort.Dispose()
}