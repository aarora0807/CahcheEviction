# launch_studio.ps1 - opens CacheSim Studio in its own program window.
# Uses Microsoft Edge (built into Windows) or Google Chrome in "app mode":
# no address bar, no tabs, its own taskbar entry. Also adds a Desktop shortcut.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$page = Join-Path $root 'web\index.html'
if (-not (Test-Path $page)) { Write-Host "Cannot find $page"; exit 1 }
$url = ([System.Uri]$page).AbsoluteUri

# Desktop shortcut (created once; recreated if the folder moved).
$desktop = [Environment]::GetFolderPath('Desktop')
$lnkPath = Join-Path $desktop 'CacheSim Studio.lnk'
$script  = Join-Path $PSScriptRoot 'launch_studio.ps1'
$shell = New-Object -ComObject WScript.Shell
$needLink = $true
if (Test-Path $lnkPath) { $needLink = ($shell.CreateShortcut($lnkPath).Arguments -notlike "*$script*") }
if ($needLink) {
    $lnk = $shell.CreateShortcut($lnkPath)
    $lnk.TargetPath = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $lnk.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
    $lnk.WorkingDirectory = $root
    $lnk.IconLocation = (Join-Path $root 'web\icon.ico')
    $lnk.WindowStyle = 7
    $lnk.Description = 'CacheSim Studio - cache eviction simulator with an AI assistant'
    $lnk.Save()
}

# Find a browser that supports app mode.
$candidates = @()
foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, $env:LOCALAPPDATA)) {
    if ($base) {
        $candidates += (Join-Path $base 'Microsoft\Edge\Application\msedge.exe')
        $candidates += (Join-Path $base 'Google\Chrome\Application\chrome.exe')
    }
}
$browser = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

# A separate profile keeps the app's settings (and your API key) apart from normal browsing.
$profileDir = Join-Path $env:LOCALAPPDATA 'CacheSimStudio'
if ($browser) {
    $browserArgs = @("--app=$url", "--user-data-dir=`"$profileDir`"", '--window-size=1440,900', '--no-first-run', '--no-default-browser-check')
    Start-Process -FilePath $browser -ArgumentList $browserArgs
} else {
    Start-Process $page
}
