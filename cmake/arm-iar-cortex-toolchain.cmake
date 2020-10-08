set(CMAKE_SYSTEM_NAME Generic)

# Set the EW installation root directory
set(EWARM_ROOT_DIR "C:/Program Files (x86)/IAR Systems/Embedded Workbench 8.4/arm")

# Do some windows specific logic
if(WIN32)
    # use the repo version of ninja on Windows as there is no Ninja installer
    set(CMAKE_MAKE_PROGRAM ${CMAKE_CURRENT_LIST_DIR}/ninja CACHE STRING "Ninja location")
endif(WIN32)

# Set up the CMake variables for compiler and assembler
set(CMAKE_C_COMPILER "${EWARM_ROOT_DIR}/bin/iccarm.exe" "${CPU_FLAGS} -e")
set(CMAKE_CXX_COMPILER "${EWARM_ROOT_DIR}/bin/iccarm.exe" "${CPU_FLAGS} --c++")
set(CMAKE_ASM_COMPILER "${EWARM_ROOT_DIR}/bin/iasmarm.exe" "${CPU_FLAGS} -r")

# Set up the CMake variables for the linker
set(CMAKE_C_LINK_FLAGS "--semihosting")
set(CMAKE_CXX_LINK_FLAGS "--semihosting")