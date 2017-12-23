# List of the ChibiOS generic STM32F4xx startup and CMSIS files.
STARTUPSRC = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/crt1.c \
             $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/vectors.c
          
STARTUPASM = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/crt0_v7m.S

STARTUPINC = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC \
             $(CHIBIOS)/os/common/startup/ARMCMx/devices/STM32F4xx \
             $(PIOS_DEVLIB)/Libraries/CMSIS/Device/ST/STM32F4xx \
             $(PIOSCOMMON)/Libraries/CMSIS/Include
#             $(CHIBIOS)/os/common/ext/CMSIS/include \
#             $(CHIBIOS)/os/common/ext/CMSIS/ST/STM32F4xx

STARTUPLD  = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/ld