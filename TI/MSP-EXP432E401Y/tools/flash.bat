:: Copyright (c) Microsoft Corporation.
:: Licensed under the MIT License.

setlocal
cd /d %~dp0\..

openocd -f board/ti_msp432_launchpad.cfg -c "program build/app/msp432e_azure_iot.elf verify reset exit"