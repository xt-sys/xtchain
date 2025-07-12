# PROJECT:     XTchain
# LICENSE:     See the COPYING.md in the top level directory
# FILE:        scripts/xtclib.ps1
# DESCRIPTION: XTchain library
# DEVELOPERS:  Aiken Harris <harraiken91@gmail.com>


# Prints XTChain banner
function banner {
    param()
    
    $XTC_BANNER = "XT Toolchain v${Env:XTCVER} for Windows"

    Write-Host "################################################################################"
    Write-Host
    Write-Host (' ' * [math]::Floor((80 - $XTC_BANNER.Length) / 2) + $XTC_BANNER) -ForegroundColor Yellow
    Write-Host
    Write-Host "################################################################################"
    Write-Host
}

# Sets the target architecture
function charch {
    param (
        [string]$arch
    )

    if ([string]::IsNullOrWhiteSpace($arch)) {
        Write-Host "Syntax: charch [architecture]"
        return
    }
    switch -Regex ($arch) {
        "aarch64|arm64" {
            $Env:TARGET = "aarch64"
        }
        "arm|armv7" {
            $Env:TARGET = "armv7"
        }
        "i386|i486|i586|i686|x86" {
            $Env:TARGET = "i686"
        }
        "amd64|x64|x86_64" {
            $Env:TARGET = "amd64"
        }
        default {
            $Env:TARGET = "UNKNOWN"
        }
    }
    Write-Host "Target Architecture: $($Env:TARGET)"
}

# Sets the build type
function chbuild {
    param (
        [string]$buildType
    )

    if ([string]::IsNullOrWhiteSpace($buildType)) {
        Write-Host "Syntax: chbuild [DEBUG|RELEASE]"
        return
    }
    switch -Regex ($buildType.ToUpper()) {
        "RELEASE" {
            $Env:BUILD_TYPE = "RELEASE"
        }
        default {
            $Env:BUILD_TYPE = "DEBUG"
        }
    }
    Write-Host "Target build type: $($Env:BUILD_TYPE)"
}

# Prints help
function help {
    banner
    Write-Host "XTChain defines an internal list of commands:"
    Write-Host " * banner          - prints XTChain banner"
    Write-Host " * charch [arch]   - sets the target CPU architecture [aarch64/armv7/i686/amd64]"
    Write-Host " * chbuild [type]  - sets build type [debug/release]"
    Write-Host " * help            - prints this message"
    Write-Host " * version         - prints XTChain and its components version"
    Write-Host " * xbuild          - builds an application with a Ninja build system"
}

# Displays version banner
function version {
    param()

    [bool]$XTCHAIN_EXTTOOLS = $false

    if ((Test-Path "${Env:XTCDIR}/bin/clang") -and 
        ((Get-Command clang).Source -eq "${Env:XTCDIR}/bin/clang") -and
        ($Env:XTCVER -match "min")) {
        $XTCHAIN_EXTTOOLS = $true
        foreach ($TOOL in @("clang", "clang++", "cmake", "lld-link", "ninja")) {
            if (!(Get-Command $TOOL -ErrorAction SilentlyContinue)) {
                Write-Error "You are using minimal version of XTChain and '${TOOL}' has been not found in your system!"
                Write-Error "Please install all required tools."
                return
            }
        }
    }

    banner
    Write-Host
    Write-Host "LLVM/Clang Compiler: $(clang --version | Select-String -Pattern "version (\d+\.\d+\.\d+)"  | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command clang).Source))"
    Write-Host "LLVM/LLD Linker: $(lld-link --version |  Select-String -Pattern "(\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command lld-link).Source))"
    Write-Host "LLVM Resource Compiler: $(windres --version | Select-String -Pattern "version (\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command windres).Source))"
    Write-Host "Wine IDL Compiler: $(widl -V | Select-String -Pattern "version (\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command widl).Source))"
    Write-Host "Wine Message Compiler: $(wmc -V | Select-String -Pattern "version (\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command wmc).Source))"
    Write-Host "Wine Resource Compiler: $(wrc --version | Select-String -Pattern "version (\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command wrc).Source))"
    Write-Host "XT SPEC Compiler: $(xtcspecc --help | Select-String -Pattern "Version (\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command xtcspecc).Source))"
    Write-Host "CMake Build System: $(cmake --version | Select-String -Pattern "version (\d+\.\d+\.\d+)"  | ForEach-Object { $_.Matches.Groups[1].Value }) ($($(Get-Command cmake).Source))"
    Write-Host "Ninja Build System: $(ninja --version) ($($(Get-Command ninja).Source))"
    Write-Host

    $BUILD_TYPE = if ($null -eq $env:BUILD_TYPE -or $env:BUILD_TYPE -eq '') { 'DEBUG' } else { $env:BUILD_TYPE }
    $TARGET = if ($null -eq $env:TARGET -or $env:TARGET -eq '') { 'amd64' } else { $env:TARGET }
    charch $TARGET
    chbuild $BUILD_TYPE

    Write-Host
    Write-Host
    Write-Host "For a list of all supported commands, type 'help'"
    Write-Host "-------------------------------------------------"
    Write-Host
    Write-Host
    Write-Host
}

# Builds application (wrapper to Ninja)
function xbuild {
    if (-not (Test-Path build.arch)) {
        & ninja @args
    } else {
        $ARCH = Get-Content build.arch
        if ($ARCH -ne $Env:TARGET) {
            Write-Host "Build is configured for '$ARCH' while current target set to '$($Env:TARGET)'!"
            Write-Host "Cannot continue until conflict is resolved ..."
            return 1
        }
        & ninja @args
    }
}
