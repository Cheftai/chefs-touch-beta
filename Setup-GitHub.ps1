$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$gitCommand = Get-Command git -ErrorAction Stop
function Run-Git {
    & $gitCommand.Source @args
    if ($LASTEXITCODE -ne 0) { throw 'Git command failed. Review the message above.' }
}
try {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git'))) {
        Run-Git init -b main
    }
    $userName = & $gitCommand.Source config user.name
    if (-not $userName) { Run-Git config user.name 'Cheftai' }
    $userEmail = & $gitCommand.Source config user.email
    if (-not $userEmail) { Run-Git config user.email '358056951+Cheftai@users.noreply.github.com' }
    Run-Git add -- .
    $changes = & $gitCommand.Source status --porcelain
    if ($changes) { Run-Git commit -m 'Prepare Chef Touch beta with Windows VST3 and remote Mac AU build' }
    $remote = $null
    if ((& $gitCommand.Source remote) -contains 'origin') { $remote = & $gitCommand.Source remote get-url origin }
    if (-not $remote) { Run-Git remote add origin 'https://github.com/Cheftai/chefs-touch-beta.git' }
    elseif ($remote -ne 'https://github.com/Cheftai/chefs-touch-beta.git') {
        throw 'Origin points at another repository. No push was performed.'
    }
    Run-Git push -u origin main
    Write-Host 'Upload complete. Open GitHub Actions to build the Mac plugin.'
} catch {
    Write-Host $_ -ForegroundColor Red
}
Read-Host 'Press Enter to close'

