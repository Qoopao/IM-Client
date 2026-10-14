param(
    [string]$Source = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-CacheValue([string]$Cache, [string]$Name)
{
    $pattern = '^' + [regex]::Escape($Name) + ':[^=]+=(.*)$'
    foreach ($line in Get-Content -LiteralPath $Cache) {
        if ($line -match $pattern) {
            return $Matches[1]
        }
    }
    return ""
}

function Stop-DeployedClient([string]$ExecutablePath)
{
    $targetPath = [IO.Path]::GetFullPath($ExecutablePath)
    foreach ($clientProcess in @(Get-Process -Name "IM-client" -ErrorAction SilentlyContinue)) {
        if (!$clientProcess.Path -or ![string]::Equals(
            [IO.Path]::GetFullPath($clientProcess.Path), $targetPath,
            [StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        Write-Host "Closing deployed client: PID $($clientProcess.Id), $targetPath"
        # Hidden/background instances have no main window, so they need termination.
        if (!$clientProcess.HasExited) {
            $closeRequested = $clientProcess.CloseMainWindow()
            if ($closeRequested) {
                $null = $clientProcess.WaitForExit(5000)
            }
        }
        if (!$clientProcess.HasExited) {
            Write-Host "Stopping remaining client process: PID $($clientProcess.Id)"
            $clientProcess.Kill()
            if (!$clientProcess.WaitForExit(5000)) {
                throw "Client process $($clientProcess.Id) did not exit. Deployment was stopped before copying files."
            }
        }
        $clientProcess.Dispose()
    }
}

function Copy-DeploymentFile([string]$File, [string]$Directory)
{
    for ($attempt = 0; $attempt -lt 10; $attempt++) {
        try {
            Copy-Item -LiteralPath $File -Destination $Directory -Force
            return
        } catch {
            $exception = $_.Exception
            while ($exception.InnerException) { $exception = $exception.InnerException }
            $win32Code = $exception.HResult -band 0xFFFF
            if ($win32Code -notin @(32, 33) -or $attempt -eq 9) { throw }
            # Windows/antivirus can briefly retain a file lock after process exit.
            Start-Sleep -Milliseconds 300
        }
    }
}

try {
    if ($Source) {
        $sourcePath = (Resolve-Path -LiteralPath $Source).Path
        if (Test-Path -LiteralPath $sourcePath -PathType Container) {
            $candidates = @(Get-ChildItem -LiteralPath $sourcePath -Filter IM-client.exe -File -Recurse)
        } else {
            $candidates = @(Get-Item -LiteralPath $sourcePath)
        }
    } else {
        $candidates = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot "build") -Filter IM-client.exe -File -Recurse)
    }

    $executable = $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (!$executable -or $executable.Name -ne "IM-client.exe") {
        throw "IM-client.exe was not found. Build the application first, or pass its path to deploy.bat."
    }

    $buildDirectory = $executable.Directory
    while ($buildDirectory -and !(Test-Path -LiteralPath (Join-Path $buildDirectory.FullName "CMakeCache.txt"))) {
        $buildDirectory = $buildDirectory.Parent
    }
    if (!$buildDirectory) {
        throw "CMakeCache.txt was not found near the executable. Pass an executable from a CMake build directory."
    }

    $cache = Join-Path $buildDirectory.FullName "CMakeCache.txt"
    $configuration = Read-CacheValue $cache "CMAKE_BUILD_TYPE"
    if (!$configuration) {
        $configuration = $executable.Directory.Name
    }
    if ($configuration -eq "Debug") {
        $configuration = "Debug"
        $deployMode = "--debug"
        $dependencySubdirectory = "debug/bin"
    } elseif ($configuration -in @("Release", "RelWithDebInfo", "MinSizeRel")) {
        $configuration = "Release"
        $deployMode = "--release"
        $dependencySubdirectory = "bin"
    } else {
        throw "Cannot determine Debug or Release configuration from the build directory."
    }

    $qtDirectory = Read-CacheValue $cache "Qt6_DIR"
    $qtRoot = [IO.Path]::GetFullPath((Join-Path $qtDirectory "../../.."))
    $qtBin = Join-Path $qtRoot "bin"
    $deployTool = Join-Path $qtBin "windeployqt.exe"
    if (!(Test-Path -LiteralPath $deployTool)) {
        throw "windeployqt.exe was not found using Qt6_DIR from CMakeCache.txt."
    }

    $installedDirectory = Read-CacheValue $cache "VCPKG_INSTALLED_DIR"
    $triplet = Read-CacheValue $cache "VCPKG_TARGET_TRIPLET"
    if (!$installedDirectory) {
        $installedDirectory = Join-Path $projectRoot "vcpkg_installed"
    }
    if (!$triplet) {
        $triplet = "x64-windows"
    }
    $dependencyBin = Join-Path (Join-Path $installedDirectory $triplet) $dependencySubdirectory
    if (!(Test-Path -LiteralPath $dependencyBin)) {
        throw "The matching vcpkg runtime directory does not exist: $dependencyBin"
    }

    $compiler = Read-CacheValue $cache "CMAKE_CXX_COMPILER"
    if ($compiler -and (Test-Path -LiteralPath $compiler)) {
        $compilerDirectory = (Get-Item -LiteralPath $compiler).Directory
        while ($compilerDirectory -and $compilerDirectory.Name -ne "VC") {
            $compilerDirectory = $compilerDirectory.Parent
        }
        if ($compilerDirectory) {
            $env:VCINSTALLDIR = $compilerDirectory.FullName + "\"
            $redistRoot = Join-Path $compilerDirectory.FullName "Redist/MSVC"
            $redistDirectory = Get-ChildItem -LiteralPath $redistRoot -Directory |
                Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
                Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
            if ($redistDirectory) {
                $env:VCToolsRedistDir = $redistDirectory.FullName + "\"
            }
        }
    }

    $outputDirectory = Join-Path $projectRoot "dist/IM-client-$configuration"
    $certificate = Join-Path $projectRoot "tools/certs/ca.crt"
    if (!(Test-Path -LiteralPath $certificate)) {
        throw "The server CA certificate is missing: $certificate"
    }
    if ($executable.FullName -eq (Join-Path $outputDirectory "IM-client.exe")) {
        throw "The deployment source must be a build executable, not the deployment output."
    }

    Write-Host "Source: $($executable.FullName)"
    Write-Host "Configuration: $configuration"
    Write-Host "Target: $outputDirectory"
    Stop-DeployedClient (Join-Path $outputDirectory "IM-client.exe")
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    Copy-DeploymentFile $executable.FullName $outputDirectory
    foreach ($dependency in Get-ChildItem -LiteralPath $dependencyBin -Filter *.dll -File) {
        Copy-DeploymentFile $dependency.FullName $outputDirectory
    }

    $env:PATH = "$qtBin;$dependencyBin;$env:PATH"
    $opensslRoot = Split-Path -Parent $dependencyBin
    & $deployTool $deployMode --force --compiler-runtime --openssl-root $opensslRoot (Join-Path $outputDirectory "IM-client.exe")
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE."
    }

    $certificateDirectory = Join-Path $outputDirectory "tools/certs"
    New-Item -ItemType Directory -Path $certificateDirectory -Force | Out-Null
    Copy-Item -LiteralPath $certificate -Destination $certificateDirectory -Force
    Set-Content -LiteralPath (Join-Path $outputDirectory "qt.conf") -Encoding ASCII -Value @("[Paths]", "Prefix=.", "Plugins=.")

    Write-Host ""
    Write-Host "Deployment completed: $outputDirectory"
    Write-Host "Launch IM-client.exe in this directory. Distribute the entire directory."
    if ($configuration -eq "Debug") {
        Write-Host "For distribution to other computers, build Release and deploy its executable."
    }
    exit 0
} catch {
    Write-Host "Deployment failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
