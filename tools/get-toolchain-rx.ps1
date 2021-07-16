# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12;

echo "`nInstalling prerequisites. Please leave the window open until the installation completes."

$gccrx_path = 'http://gcc-renesas.com/downloads/get.php?f=rx/8.3.0.202004-gnurx'
$gccrx_file = 'gcc-8.3.0.202004-GNURX-ELF.exe'
$gccrx_name = 'GCC-RX 8.3.0.20204'
$gccrx_hash = 'C3340B4915208361A33885505CFB89B69E1607A07F89FBF2A79096182D293978'

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

echo "(1/3) $gccrx_name"
if ( -not (Test-Path "$env:TEMP\$gccrx_file") -Or ((Get-FileHash "$env:TEMP\$gccrx_file").Hash -ne $gccrx_hash))
{
    $wc.DownloadFile("$gccrx_path/$gccrx_file", "$env:TEMP\$gccrx_file")
}

echo "(2/3) $cmake_name"
if ( -not (Test-Path "$env:TEMP\$cmake_file") -Or ((Get-FileHash "$env:TEMP\$cmake_file").Hash -ne $cmake_hash))
{
    $wc.DownloadFile("$cmake_path/$cmake_file", "$env:TEMP\$cmake_file")
}

echo "(3/3) $termite_name"
if ( -not (Test-Path "$env:TEMP\$termite_file") -Or ((Get-FileHash "$env:TEMP\$termite_file").Hash -ne $termite_hash))
{
    $wc.DownloadFile("$termite_path/$termite_file", "$env:TEMP\$termite_file")
}

echo "`nInstalling packages..."

echo "(1/3) $gccrx_name"
Start-Process -FilePath "$env:TEMP\$gccrx_file" -ArgumentList "/SILENT" -Wait

echo "(2/3) $cmake_name"
Start-Process -FilePath "$env:TEMP\$cmake_file" -ArgumentList "ADD_CMAKE_TO_PATH=System /passive" -Wait

echo "(3/3) $termite_name"
Start-Process -FilePath "$env:TEMP\$termite_file" -ArgumentList "/S" -Wait

echo "`nInstallation complete!"

echo "`nPress any key to continue..."
Read-Host
