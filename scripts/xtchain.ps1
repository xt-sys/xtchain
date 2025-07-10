# PROJECT:     XTchain
# LICENSE:     See the COPYING.md in the top level directory
# FILE:        scripts/xtchain.ps1
# DESCRIPTION: XTchain Entry Script
# DEVELOPERS:  Aiken Harris <harraiken91@gmail.com>

# Get the absolute path to the XTchain
$XTCDIR = (Get-Item -Path ".\").FullName

# Read the XTchain version
$env:XTCVER = Get-Content "${XTCDIR}\Version"

# Load the library (Make sure the xtclib.ps1 file is PowerShell compatible)
. "${XTCDIR}\lib\xtchain\xtclib.ps1"

# Set the target architecture
$env:TARGET = $args[0]
if (-not $env:TARGET) { $env:TARGET = "amd64" }

# Save the source directory
$SRCDIR = $args[1]
if (-not $SRCDIR) { $SRCDIR = (Get-Location).Path }

# Make sure the compiler flags are clean
$env:HOST = $null
$env:CFLAGS = $null
$env:CXXFLAGS = $null
$env:LDFLAGS = $null

# Update PATH
$env:PATH = "${XTCDIR}\bin;" + $env:PATH

# Display banner
version

# Invoke shell with fancy prompt
function global:prompt {
    $PROMPT = " XT Toolchain "
    $CWD = (Get-Location).Path
    $CHEVRON = [char]0xE0B0
    $SEGMENTS = @(
        @{ TEXT = $PROMPT; BGCOLOR = "Blue"; FGCOLOR = "White" },
        @{ TEXT = " $CWD ";     BGCOLOR = "DarkCyan";     FGCOLOR = "White" }
    )
    for ($INDEX = 0; $INDEX -lt $SEGMENTS.Count; $INDEX++) {
        $SEGMENT = $SEGMENTS[$INDEX]
        $NEXTBG = if ($INDEX + 1 -lt $SEGMENTS.Count) { $SEGMENTS[$INDEX + 1].BGCOLOR } else { "Default" }
        Write-Host $SEGMENT.TEXT -NoNewLine -ForegroundColor $SEGMENT.FGCOLOR -BackgroundColor $SEGMENT.BGCOLOR
        if ($NEXTBG -ne "Default") {
            Write-Host $CHEVRON -NoNewLine -ForegroundColor $SEGMENT.BGCOLOR -BackgroundColor $NEXTBG
        } else {
            Write-Host $CHEVRON -NoNewLine -ForegroundColor $SEGMENT.BGCOLOR
        }
    }
    return " "
}
Set-Location -Path $SRCDIR
