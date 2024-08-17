function Show-MessageBox {
    param ([string]$message)
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show($message, "Bootstrap Error", 'OK', 'Warning')
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Show-MessageBox "Git is needed to checkout vcpkg, but it's not installed or not available in PATH."
    exit 1
}

git submodule update --init --recursive

if (-not (Test-Path "vcpkg/.git")) {
    if (-not (Test-Path "version.json")) {
        Show-MessageBox "Unable to checkout correct version of vcpkg without 'version.json'."
        exit 1
    }

    try {
        $versionData = Get-Content -Raw -Path "version.json" | ConvertFrom-Json
    } catch {
        Show-MessageBox "version.json is not valid JSON."
        exit 1
    }

    $vcpkgVersion = $versionData.vcpkg

    if (Test-Path "vcpkg") {
        Remove-Item -Recurse -Force "vcpkg"
    }

    git clone --branch $vcpkgVersion --depth 1 https://github.com/microsoft/vcpkg.git "vcpkg"
    $cloneExitCode = $LASTEXITCODE

    if ($cloneExitCode -ne 0) {
        Show-MessageBox "Checkout of vcpkg failed."
        exit 1
    }
}

& "vcpkg/bootstrap-vcpkg.bat" -disableMetrics