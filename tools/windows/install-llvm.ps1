# SPDX-License-Identifier: GPL-3.0-or-later

# Installs the LLVM release that the Windows CI jobs compile with (clang-cl, lld-link, llvm-lib)
# and exports its location as LLVM_ROOT for later steps. The installer is pinned by checksum so a
# runner image update never changes the compiler.

$ErrorActionPreference = 'Stop'

$version = '22.1.8'
$sha256 = '16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c'

$installer = Join-Path $env:RUNNER_TEMP 'llvm.exe'
$url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$version/LLVM-$version-win64.exe"
# GitHub release downloads sometimes answer 504 for a minute or two; retry instead of failing.
$attempts = 8
for ($attempt = 1; ; $attempt++) {
    try {
        Invoke-WebRequest -OutFile $installer $url
        if ((Get-FileHash -Algorithm SHA256 $installer).Hash -eq $sha256) {
            break
        }
        throw 'LLVM installer checksum mismatch'
    } catch {
        if ($attempt -ge $attempts) {
            throw
        }
        $wait = [Math]::Min(120, 15 * $attempt)
        Write-Host "LLVM download attempt $attempt failed ($($_.Exception.Message)); retrying in $wait s"
        Start-Sleep -Seconds $wait
    }
}
$root = Join-Path $env:RUNNER_TEMP 'llvm'
Start-Process -Wait -FilePath $installer -ArgumentList '/S', "/D=$root"
$reported = & "$root/bin/clang-cl.exe" --version
$reported
if (-not ($reported -match "clang version $([regex]::Escape($version))")) {
    throw "Unexpected clang-cl: $reported"
}
"LLVM_ROOT=$($root.Replace('\', '/'))" | Out-File -Append -Encoding utf8 $env:GITHUB_ENV
