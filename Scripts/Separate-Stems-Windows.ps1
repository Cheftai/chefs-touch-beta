param([string]$InputWav, [string]$OutputFolder)
$ErrorActionPreference = 'Stop'
try {
    if ($env:OS -ne 'Windows_NT') { throw 'Run separation on Windows; transfer finished WAV files to Mac.' }
    if (-not $InputWav) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Filter = 'WAV audio (*.wav)|*.wav'
        if ($dialog.ShowDialog() -ne 'OK') { exit 0 }
        $InputWav = $dialog.FileName
    }
    $InputWav = (Resolve-Path -LiteralPath $InputWav).Path
    if ([IO.Path]::GetExtension($InputWav) -ne '.wav') { throw 'Export your song as WAV first.' }
    if (-not $OutputFolder) { $OutputFolder = Join-Path ([IO.Path]::GetDirectoryName($InputWav)) 'ChefsTouch-Stems' }
    New-Item -ItemType Directory -Force -Path $OutputFolder | Out-Null
    $OutputFolder = (Resolve-Path -LiteralPath $OutputFolder).Path
    $tools = Join-Path $PSScriptRoot '.stem-tools'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $uv = Join-Path $tools 'uv.exe'
    if (-not (Test-Path -LiteralPath $uv)) {
        Write-Host 'Downloading official uv environment manager...'
        $release = Invoke-RestMethod 'https://api.github.com/repos/astral-sh/uv/releases/latest'
        $asset = $release.assets | Where-Object name -EQ 'uv-x86_64-pc-windows-msvc.zip' | Select-Object -First 1
        if (-not $asset) { throw 'Official uv Windows download not found.' }
        $zip = Join-Path $tools 'uv.zip'
        Invoke-WebRequest $asset.browser_download_url -OutFile $zip
        Expand-Archive -LiteralPath $zip -DestinationPath $tools -Force
        $found = Get-ChildItem -LiteralPath $tools -Recurse -Filter uv.exe | Select-Object -First 1
        if (-not $found) { throw 'uv extraction failed.' }
        if ($found.FullName -ne $uv) { Copy-Item -LiteralPath $found.FullName -Destination $uv }
    }
    $environment = Join-Path $tools 'python-env'
    $python = Join-Path $environment 'Scripts\python.exe'
    if (-not (Test-Path -LiteralPath $python)) {
        & $uv venv --python 3.11 $environment
        if ($LASTEXITCODE -ne 0) { throw 'Python setup failed.' }
    }
    $ready = Join-Path $environment 'chefs-ready.txt'
    if (-not (Test-Path -LiteralPath $ready)) {
        Write-Host 'Installing CPU inference tools on Windows. First run requires internet and substantial disk space.'
        & $uv pip install --python $python --index-url 'https://download.pytorch.org/whl/cpu' 'torch==2.5.1' 'torchaudio==2.5.1'
        if ($LASTEXITCODE -ne 0) { throw 'CPU PyTorch installation failed.' }
        & $uv pip install --python $python 'demucs==4.0.1' soundfile
        if ($LASTEXITCODE -ne 0) { throw 'Demucs installation failed.' }
        'Demucs 4.0.1 / torch 2.5.1 CPU' | Set-Content -LiteralPath $ready
    }
    $track = Join-Path (Join-Path $OutputFolder 'htdemucs') ([IO.Path]::GetFileNameWithoutExtension($InputWav))
    if (Test-Path -LiteralPath $track) { throw 'Output already exists. Choose another output folder to preserve previous stems.' }
    Write-Host 'Separating locally on Windows CPU. Model weights download on first use. Audio stays on this PC.'
    & $python -m demucs -n htdemucs -d cpu --shifts 1 --float32 --out $OutputFolder -- $InputWav
    if ($LASTEXITCODE -ne 0) { throw 'Separation failed; see the messages above.' }
    foreach ($stem in @('vocals','drums','bass','other')) {
        if (-not (Test-Path -LiteralPath (Join-Path $track "$stem.wav"))) { throw "Missing stem: $stem" }
    }
    'Transfer the four WAVs to Mac. Import all at the same DAW start position. Keep the original mix; separation can contain bleed/artifacts. Mac needs no model or Python.' | Set-Content -LiteralPath (Join-Path $track 'MAC-IMPORT.txt')
    Start-Process explorer.exe -ArgumentList ('"' + $track + '"')
    Write-Host "Finished: $track" -ForegroundColor Green
} catch {
    Write-Host $_ -ForegroundColor Red
    exit 1
}
