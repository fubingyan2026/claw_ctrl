param(
    [string]$Objcopy,
    [string]$FlashBase,
    [string]$FlashSize,
    [string]$AppBase,
    [string]$AppSize,
    [string]$MetadataBase,
    [string]$HeaderOffset,
    [string]$AppMagic,
    [string]$AppTarget,
    [string]$VersionMagic
)
$ErrorActionPreference = 'Stop'

function Get-Crc32([byte[]]$Data) {
    [uint32]$crc = [uint32]::MaxValue
    [uint32]$poly = 3988292384 # 0xEDB88320
    foreach ($byte in $Data) {
        $crc = $crc -bxor $byte
        for ($bit = 0; $bit -lt 8; $bit++) {
            $low = $crc -band 1
            $crc = $crc -shr 1
            if ($low) { $crc = $crc -bxor $poly }
        }
    }
    return ($crc -bxor [uint32]::MaxValue)
}

function Test-Vectors([byte[]]$Image, [uint32]$Base, [uint32]$CodeStart) {
    if ($Image.Length -lt 8) { return $false }
    $sp = [BitConverter]::ToUInt32($Image, 0)
    $entry = [BitConverter]::ToUInt32($Image, 4)
    return ($sp -gt 0x20000000 -and $sp -le 0x20010000 -and ($sp -band 7) -eq 0 -and
        ($entry -band 1) -eq 1 -and ($entry - 1) -ge $CodeStart -and ($entry - 1) -lt ($Base + $Image.Length))
}

$base = [Convert]::ToUInt32($FlashBase, 16)
$size = [Convert]::ToUInt32($FlashSize, 16)
$appAddr = [Convert]::ToUInt32($AppBase, 16)
$appCapacity = [Convert]::ToUInt32($AppSize, 16)
$metaOffset = [Convert]::ToUInt32($MetadataBase, 16) - $base
$header = [Convert]::ToUInt32($HeaderOffset, 16)
$boot = [IO.File]::ReadAllBytes((Resolve-Path 'claw_boot.bin').Path)
$app = [IO.File]::ReadAllBytes((Resolve-Path 'claw_ctrl.bin').Path)

if ($boot.Length -gt ($appAddr - $base) -or !(Test-Vectors $boot $base $base)) {
    throw 'Invalid claw_boot.bin size or vectors'
}
if ($app.Length -lt ($header + 16) -or $app.Length -gt $appCapacity -or
    !(Test-Vectors $app $appAddr ($appAddr + $header + 16)) -or
    [BitConverter]::ToUInt32($app, $header) -ne [Convert]::ToUInt32($AppMagic, 16) -or
    [BitConverter]::ToUInt32($app, $header + 4) -ne [Convert]::ToUInt32($AppTarget, 16) -or
    [BitConverter]::ToUInt32($app, $header + 8) -ne [Convert]::ToUInt32($VersionMagic, 16) -or
    [BitConverter]::ToUInt32($app, $header + 12) -gt 0xffffff) {
    throw 'Invalid claw_ctrl.bin size, vectors, target or version'
}

# ring_storage frame v1 with the image and stats KVs. Values are little endian.
$key = [Text.Encoding]::ASCII.GetBytes('image')
$statsKey = [Text.Encoding]::ASCII.GetBytes('stats')
$stream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([uint32]0x52535446)
$writer.Write([uint32]1)
$writer.Write([uint32](28 + 3 + $key.Length + 24 + 3 + $statsKey.Length + 8))
$writer.Write([uint32]2)
$writer.Write([uint32](Get-Crc32 $stream.ToArray()))
$writer.Write([byte]$key.Length)
$writer.Write($key)
$writer.Write([uint16]24)
$writer.Write([uint32]$app.Length)
$writer.Write([uint32](Get-Crc32 $app))
$writer.Write([BitConverter]::ToUInt32($app, $header + 12))
$writer.Write([byte[]]::new(12)) # No pending image.
$writer.Write([byte]$statsKey.Length)
$writer.Write($statsKey)
$writer.Write([uint16]8)
$writer.Write([byte[]]::new(8))
$data = $stream.ToArray()
$writer.Write([uint32](Get-Crc32 ($data[20..($data.Length - 1)])))
$writer.Write([uint32]0x434F4D54)
$metadata = $stream.ToArray()
$writer.Dispose()

$image = [byte[]]::new($size)
$erasedPage = [byte[]]::new(2048)
for ($i = 0; $i -lt $erasedPage.Length; $i++) { $erasedPage[$i] = 255 }
for ($i = 0; $i -lt $size; $i += $erasedPage.Length) {
    [Array]::Copy($erasedPage, 0, $image, $i, $erasedPage.Length)
}
[Array]::Copy($boot, 0, $image, 0, $boot.Length)
[Array]::Copy($app, 0, $image, ($appAddr - $base), $app.Length)
[Array]::Copy($metadata, 0, $image, $metaOffset, $metadata.Length)
[IO.File]::WriteAllBytes((Join-Path (Get-Location) 'claw_factory.bin'), $image)

& $Objcopy -I binary -O ihex --change-addresses $FlashBase claw_factory.bin claw_factory.hex
if ($LASTEXITCODE -ne 0) { throw 'Cannot convert factory image to Intel HEX' }

@"
ExitOnError 1
erase
loadfile claw_factory.bin $FlashBase
r
g
exit
"@ | Set-Content -Encoding ASCII claw_factory.jlink
