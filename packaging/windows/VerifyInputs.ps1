param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$Bundle,
    [Parameter(Mandatory = $true)][string]$Redist,
    [Parameter(Mandatory = $true)][string]$Product,
    [Parameter(Mandatory = $true)][string]$PluginVersion,
    [Parameter(Mandatory = $true)][string]$OutputFile
)

$ErrorActionPreference = 'Stop'
Import-Module "$PSHOME\Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1"
Import-Module "$PSHOME\Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1"
$binary = Join-Path $Bundle "Contents\x86_64-win\$Product.vst3"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    throw "Built x64 VST3 binary not found: $binary"
}
if ((Get-Item -LiteralPath $binary).VersionInfo.FileVersion -ne $PluginVersion) {
    throw "Plugin version does not match release version $PluginVersion. Rebuild the plugin with the matching version."
}
if ((Get-Item -LiteralPath $Bundle).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw "The input bundle must not be a link: $Bundle"
}
Get-ChildItem -LiteralPath $Bundle -Recurse -Force | ForEach-Object {
    if ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "The input bundle contains a link: $($_.FullName)"
    }
}
$compilerFiles = @(Get-ChildItem -LiteralPath (Join-Path $BuildDirectory 'CMakeFiles') -Filter CMakeCXXCompiler.cmake -Recurse)
if ($compilerFiles.Count -ne 1) {
    throw 'Expected exactly one compiler metadata file in the application build directory.'
}
$compiler = Select-String -LiteralPath $compilerFiles[0].FullName -Pattern '^set\(CMAKE_CXX_COMPILER_VERSION "19\.(\d+)\.(\d+)\.(\d+)"\)$'
if (-not $compiler) {
    throw 'The application build must use an identifiable MSVC compiler.'
}
$minimum = [version]::new(14, [int]$compiler.Matches[0].Groups[1].Value, [int]$compiler.Matches[0].Groups[2].Value, [int]$compiler.Matches[0].Groups[3].Value)
$expectedHash = '9C02C9ED120F671B900E6B9471D890E3B2414117A7E53F4AE2E855466FA2B7C9'
if ((Get-FileHash -LiteralPath $Redist -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'The redistributable does not match the pinned SHA-256.'
}
$signature = Get-AuthenticodeSignature -LiteralPath $Redist
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '^CN=Microsoft Corporation,') {
    throw 'The redistributable does not have a valid Microsoft signature.'
}
$version = [version](Get-Item -LiteralPath $Redist).VersionInfo.FileVersion
if ($version -lt $minimum) {
    throw "The redistributable ($version) is older than the compiler runtime requirement ($minimum)."
}
[IO.File]::WriteAllText($OutputFile, $minimum.ToString())
