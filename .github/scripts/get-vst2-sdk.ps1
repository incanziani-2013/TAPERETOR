# Downloads the VST2 SDK from a repository secret so the build can include VST2.
# Set ONE of these secrets (Settings > Secrets and variables > Actions):
#   VST2_SDK_URL         direct download link to a .zip of the SDK
#   VST2_SDK_ZIP_BASE64  base64 of a .zip of the SDK (only pluginterfaces/vst2.x is needed)
# Without either secret, VST2 is skipped and only VST3 is built.
$ErrorActionPreference = 'Stop'

$zip  = Join-Path $env:RUNNER_TEMP 'vst2sdk.zip'
$dest = Join-Path $env:RUNNER_TEMP 'vst2sdk'

if ($env:VST2_SDK_URL) {
    Invoke-WebRequest -Uri $env:VST2_SDK_URL -OutFile $zip
} elseif ($env:VST2_SDK_ZIP_BASE64) {
    [IO.File]::WriteAllBytes($zip, [Convert]::FromBase64String($env:VST2_SDK_ZIP_BASE64))
} else {
    Write-Output '::warning::No VST2 SDK secret set (VST2_SDK_URL or VST2_SDK_ZIP_BASE64): building VST3 only.'
    exit 0
}

Expand-Archive -Path $zip -DestinationPath $dest -Force
$header = Get-ChildItem -Path $dest -Recurse -Filter 'aeffect.h' |
    Where-Object { $_.Directory.Name -eq 'vst2.x' -and $_.Directory.Parent.Name -eq 'pluginterfaces' } |
    Select-Object -First 1
if (-not $header) {
    Write-Output '::error::The VST2 SDK zip does not contain pluginterfaces/vst2.x/aeffect.h'
    exit 1
}

$root = $header.Directory.Parent.Parent.FullName
Write-Output "VST2 SDK found at $root"
"VST2_SDK_PATH=$root" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
