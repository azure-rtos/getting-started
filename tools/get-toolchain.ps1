# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12;

echo "`nInstalling prerequisites. Please leave the window open until the installation completes."

$gccarm_path = 'https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/'
$gccarm_file = 'gcc-arm-none-eabi-10-2020-q4-major-win32.exe'
$gccarm_name = 'GCC-ARM 10-2020-q4-major'
$gccarm_hash = '9754D95905288AFC787837C3B211A3E502A9EDE8A5F544CFD369C30FEA509CA5'

$cmake_path = 'https://github.com/Kitware/CMake/releases/download/v3.20.3/'
$cmake_file = 'cmake-3.20.3-windows-i386.msi'
$cmake_name = 'CMake v3.20.3'
$cmake_hash = '583A6A5C896251F25F367FD27FC54C470E01D11E82B4F19B28BF93B549AC3EAD'

$termite_path = 'https://www.compuphase.com/software'
$termite_file = 'termite-3.4.exe'
$termite_name = 'Termite v3.4'
$termite_hash = 'CA440B6C7F6EAA812BA5F8BF42AED86E02022CA50A1C72585168C9B671D0FE19'

echo "`nDownloading packages..."

$wc = New-Object System.Net.WebClient
$wc.Headers['User-Agent'] = "Mozilla/4.0"

echo "(1/3) $gccarm_name"
if ( -not (Test-Path "$env:TEMP\$gccarm_file") -Or ((Get-FileHash "$env:TEMP\$gccarm_file").Hash -ne $gccarm_hash))
{
    $wc.DownloadFile("$gccarm_path\$gccarm_file", "$env:TEMP\$gccarm_file")
}

echo "(2/3) $cmake_name"
if ( -not (Test-Path "$env:TEMP\$cmake_file") -Or ((Get-FileHash "$env:TEMP\$cmake_file").Hash -ne $cmake_hash))
{
    $wc.DownloadFile("$cmake_path\$cmake_file", "$env:TEMP\$cmake_file")
}

echo "(3/3) $termite_name"
if ( -not (Test-Path "$env:TEMP\$termite_file") -Or ((Get-FileHash "$env:TEMP\$termite_file").Hash -ne $termite_hash))
{
    $wc.DownloadFile("$termite_path\$termite_file", "$env:TEMP\$termite_file")
}

echo "`nInstalling packages..."

echo "(1/3) $gccarm_name"
Start-Process -FilePath "$env:TEMP\$gccarm_file" -ArgumentList "/S /P /R" -Wait

echo "(2/3) $cmake_name"
Start-Process -FilePath "$env:TEMP\$cmake_file" -ArgumentList "ADD_CMAKE_TO_PATH=System /passive" -Wait

echo "(3/3) $termite_name"
Start-Process -FilePath "$env:TEMP\$termite_file" -ArgumentList "/S" -Wait

echo "`nInstallation complete!"

echo "`nPress any key to continue..."
Read-Host
