# Shared allowlist for this project's C/ASM, Cortex-M33 FPv5 hard-float runtime.
function Get-PortableToolFiles {
    param([string]$ProjectRoot, [switch]$FullTools)
    $armPackage='arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi'
    foreach ($tool in @($armPackage,'cmake-3.31.6-windows-x86_64','ninja-1.12.1')) {
        $root=Join-Path $ProjectRoot ".tools/$tool"
        foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Force) {
            $rel=$file.FullName.Substring($root.Length+1).Replace('\','/')
            $keep=$FullTools -or $tool -eq 'ninja-1.12.1'
            # Retain upstream license/copyright notices even outside runtime trees.
            $keep=$keep -or $file.Name -match '(?i)copying|copyright|licen[cs]e'
            if ($tool -eq $armPackage) {
                $keep=$keep -or $rel -match '^bin/arm-none-eabi-(gcc|gdb|ar|as|ld|ld.bfd|ranlib|nm|objcopy|objdump|size|readelf|addr2line|strip)\.exe$'
                $keep=$keep -or $rel -match '^arm-none-eabi/bin/(as|ld|ld.bfd)\.exe$'
                $keep=$keep -or ($rel -match '^arm-none-eabi/include/' -and $rel -notmatch '^arm-none-eabi/include/c\+\+/')
                $keep=$keep -or $rel -match '^libexec/gcc/arm-none-eabi/14\.2\.1/(cc1|collect2|lto-wrapper|lto1)\.exe$'
                $keep=$keep -or $rel -match '^libexec/gcc/arm-none-eabi/14\.2\.1/liblto_plugin\.dll$'
                $keep=$keep -or $rel -match '^lib/gcc/arm-none-eabi/14\.2\.1/(include|include-fixed)/'
                $keep=$keep -or $rel -match '^lib/gcc/arm-none-eabi/14\.2\.1/[^/]+\.(o|specs)$'
                $keep=$keep -or $rel -eq 'lib/gcc/arm-none-eabi/14.2.1/libgcc.a'
                if ($rel -match '^lib/gcc/arm-none-eabi/14\.2\.1/thumb/v8-m\.main\+fp/hard/[^/]+$') {
                    $keep=$keep -or ($file.Name -notmatch '^(libstdc\+\+|libsupc\+\+|libgfortran|libcaf)' -and $file.Extension -in @('.a','.o','.specs'))
                }
                $keep=$keep -or $rel -match '^share/gdb/'
                $keep=$keep -or $rel -eq 'share/doc/arm-none-eabi/readme.txt'
                $keep=$keep -or $rel -in @('.version','manifest.txt')
            } elseif ($tool -eq 'cmake-3.31.6-windows-x86_64') {
                $keep=$keep -or $rel -match '^bin/(cmake|ctest|cmcldeps)\.exe$'
                $keep=$keep -or $rel -match '^share/cmake-3\.31/(Modules|Templates)/'
                $keep=$keep -or $rel -match '^share/cmake-3\.31/Licenses/'
            }
            if ($keep) { $file }
        }
    }
}
