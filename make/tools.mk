###############################################################
#
# Installers for tools required by the ground and flight builds
#
# NOTE: These are not tied to the default goals
#       and must be invoked manually
#
###############################################################

##############################
#
# Check that environmental variables are sane
#
##############################
# Checking for $(OPENOCD_FTDI) to be sane
ifdef OPENOCD_FTDI
  ifneq ($(OPENOCD_FTDI),yes)
    ifneq ($(OPENOCD_FTDI),no)
      $(error Only yes or no are allowed for OPENOCD_FTDI)
    endif
  endif
endif

# Change the Qt version here, format is e.g. 5.8.0, 5.8.0-1 or 5.8.0-beta etc.

QT_VERSION := 5.9.2
QT_MINGW_VERSION := 530

# These bits are not user serviceable
QT_VERSION_LONG := $(word 1, $(subst -, , $(QT_VERSION)))
ifneq ($(words $(subst ., , $(QT_VERSION_LONG))),3)
  $(error Invalid Qt version!)
endif
QT_VERSION_SUFFIX := $(word 2, $(subst -, ,$(QT_VERSION)))
ifneq ($(subst _, ,$(QT_VERSION_SUFFIX)), 1)
  QT_VERSION_EXTRA := $(word 2, $(subst _, ,$(QT_VERSION_SUFFIX)))
  QT_VERSION_SUFFIX := $(word 1, $(subst _, ,$(QT_VERSION_SUFFIX)))
endif
QT_VERSION_FULL := $(QT_VERSION_LONG)
ifneq ($(strip $(QT_VERSION_SUFFIX)),)
  QT_VERSION_FULL := $(QT_VERSION_FULL)-$(QT_VERSION_SUFFIX)
endif
ifneq ($(strip $(QT_VERSION_EXTRA)),)
  QT_VERSION_EXTRA := _$(strip $(QT_VERSION_EXTRA))
else
  QT_VERSION_EXTRA :=
endif
QT_VERSION_SHORT := $(word 1, $(subst ., , $(QT_VERSION_FULL))).$(word 2, $(subst ., , $(QT_VERSION_FULL)))
ifeq ($(words $(filter alpha beta rc, $(QT_VERSION_SUFFIX))), 1)
  QT_VERSION_SOURCE := development
else
  QT_VERSION_SOURCE := official
endif
QT_PKG_VERSION := $(subst .,,$(QT_VERSION_LONG))
QT_SDK_DIR := $(TOOLS_DIR)/Qt$(QT_VERSION_FULL)


.PHONY: tools_required_qt
ifdef IGNORE_MISSING_TOOLCHAIN
tools_required_qt:
	$(warning "Skipping Qt toolchain check!")
else
ifdef WINDOWS
tools_required_qt:
	$(V1) echo "*** Not checking Qt version on Windows ***"
else
tools_required_qt:
ifeq ($(wildcard $(QT_SDK_DIR)/*),)
	$(error "Qt SDK not found, please run `make qt_sdk_install`")
else
	$(info "Qt SDK found in $(QT_SDK_DIR)")
endif
endif # WINDOWS
endif # IGNORE_MISSING_TOOLCHAIN

ifdef LINUX
  ifdef AMD64
    QT_PLUGINS_DIR = $(QT_SDK_DIR)/$(QT_VERSION_FULL)/gcc_64/plugins
  else
    QT_PLUGINS_DIR = $(QT_SDK_DIR)/$(QT_VERSION_FULL)/gcc/plugins
  endif
endif

# Set variables for Android SDK
ifdef MACOSX
  ANDROID_SDK_DIR := $(TOOLS_DIR)/android-sdk-macosx
  ANDROID_SDK_URL := http://dl.google.com/android/android-sdk_r23.0.2-macosx.zip
endif

ifdef LINUX
  ANDROID_SDK_DIR := $(TOOLS_DIR)/android-sdk-linux
  ANDROID_SDK_URL := http://dl.google.com/android/android-sdk_r23.0.2-linux.tgz
endif

ifdef WINDOWS
  ANDROID_SDK_DIR := $(TOOLS_DIR)/android-sdk-windows
  ANDROID_SDK_URL := http://dl.google.com/android/android-sdk_r23.0.2-windows.zip
endif

# Build openocd without FTDI (yes | no)
OPENOCD_FTDI ?= yes


# $1 URL
# $2 save dir
# $3 save filename
define DOWNLOAD_template
  $(V1) (\
    if [ -e "$(2)/$(3)" ]; then \
      curl -z "$(2)/$(3)" -L -k -o "$(2)/$(3)" "$(1)"; \
    else \
      curl -L -k -o "$(2)/$(3)" "$(1)"; \
    fi;\
  )
endef


.PHONY: qt_sdk_install

# QT SDK download URL
# QT_URL_PREFIX := http://download.qt.io/$(QT_VERSION_SOURCE)_releases/qt/$(QT_VERSION_SHORT)/$(QT_VERSION_FULL)/qt-opensource-
QT_URL_PREFIX := https://download.qt.io/archive/qt/$(QT_VERSION_SHORT)/$(QT_VERSION_FULL)/qt-opensource-
$(info $$QT_URL_PREFIX is [${QT_URL_PREFIX}])
ifdef LINUX
  ifdef AMD64
    # Linux 64-bit
    qt_sdk_install: QT_SDK_URL := $(QT_URL_PREFIX)linux-x64-$(QT_VERSION_FULL)$(QT_VERSION_EXTRA).run
    QT_SDK_QMAKE_PATH := $(QT_SDK_DIR)/$(QT_VERSION_FULL)/gcc_64/bin/qmake
    QT_SDK_QBS_PATH := $(QT_SDK_DIR)/Tools/QtCreator/bin/qbs
    QBS_PROFILE := gcc
  else
    $(warning Build is only supported on 64-bit Linux)
  endif
endif

ifdef MACOSX
  qt_sdk_install: QT_SDK_URL  := $(QT_URL_PREFIX)mac-x64-$(QT_VERSION_FULL)$(QT_VERSION_EXTRA).dmg
  QT_SDK_QMAKE_PATH := $(QT_SDK_DIR)/$(QT_VERSION_FULL)/clang_64/bin/qmake
  QT_SDK_QBS_PATH := $(QT_SDK_DIR)/Qt\ Creator.app/Contents/MacOS/qbs
  QBS_PROFILE := clang
  export QT_SDK_BIN_PATH := $(QT_SDK_DIR)/$(QT_VERSION_FULL)/clang_64/bin
endif

ifdef WINDOWS
  qt_sdk_install: QT_SDK_URL  := $(QT_URL_PREFIX)windows-x86-$(QT_VERSION_FULL)$(QT_VERSION_EXTRA).exe
  QT_SDK_QBS_PATH := $(QT_SDK_DIR)/Tools/QtCreator/bin/qbs
  ifeq ($(USE_MSVC),YES)
    QBS_PROFILE := MSVC2015-x86
  endif
  QT_SDK_QMAKE_PATH := $(QT_SDK_DIR)/$(QT_VERSION_FULL)/mingw$(QT_MINGW_VERSION)_32/bin/qmake
endif

qt_sdk_install: QT_SDK_FILE := $(notdir $(QT_SDK_URL))

# order-only prereq on directory existance:
qt_sdk_install : | $(DL_DIR) $(TOOLS_DIR)
qt_sdk_install: qt_sdk_clean
        # download the source only if it's newer than what we already have
	$(call DOWNLOAD_template,$(QT_SDK_URL),$(DL_DIR),$(QT_SDK_FILE))
        # tell the user exactly which path they should select in the GUI
	$(V1) echo "*** NOTE NOTE NOTE ***"
	$(V1) echo "*"
	$(V1) echo "*  In the GUI, please use exactly this path as the installation path:"
	$(V1) echo "*        $(QT_SDK_DIR)"
	$(V1) echo "*"
	$(V1) echo "*** NOTE NOTE NOTE ***"

ifneq (,$(filter $(UNAME), Darwin))
	$(V1) hdiutil attach -quiet -private -mountpoint /tmp/qt-installer "$(DL_DIR)/$(QT_SDK_FILE)"
	$(V1) /tmp/qt-installer/qt-opensource-mac-x64-$(QT_VERSION_LONG).app/Contents/MacOS/qt-opensource-mac-x64-$(QT_VERSION_LONG) \
		--script $(ROOT_DIR)/make/scripts/qt-install.qs \
		dr_qt_path="$(QT_SDK_DIR)" \
		dr_qt_ver=$(QT_PKG_VERSION) \
		dr_packages="qtcharts,clang_64"
	$(V1) hdiutil detach -quiet /tmp/qt-installer
endif

ifneq (,$(filter $(UNAME), Linux))
        #installer is an executable, make it executable and run it
	$(V1) chmod u+x "$(DL_DIR)/$(QT_SDK_FILE)"
	$(V1) "$(DL_DIR)/$(QT_SDK_FILE)" \
		--script $(ROOT_DIR)/make/scripts/qt-install.qs \
		dr_qt_path="$(QT_SDK_DIR)" \
		dr_qt_ver=$(QT_PKG_VERSION) \
		dr_packages="qtcharts,gcc_64"
endif

ifdef WINDOWS
	$(V1) "$(DL_DIR)/$(QT_SDK_FILE)" \
		--script $(ROOT_DIR)/make/scripts/qt-install.qs \
		dr_qt_path="C:\\Qt\\Qt$(QT_VERSION_LONG)" \
		dr_qt_ver=$(QT_PKG_VERSION) \
		dr_packages="qtcharts,win32_msvc2015,qt.tools.win32_mingw530"
endif

.PHONY: qt_sdk_clean
qt_sdk_clean:
	$(V1) [ ! -d "$(QT_SDK_DIR)" ] || $(RM) -rf $(QT_SDK_DIR)

# Set up ARM (STM32) SDK
ARM_SDK_DIR := $(TOOLS_DIR)/gcc-arm-none-eabi-6-2017-q2-update

.PHONY: arm_sdk_install
ifdef LINUX
  arm_sdk_install: ARM_SDK_URL  := https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2
endif

ifdef MACOSX
  arm_sdk_install: ARM_SDK_URL  := https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-mac.tar.bz2
endif

ifdef WINDOWS
  arm_sdk_install: ARM_SDK_URL  := https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-win32.zip
endif

arm_sdk_install: ARM_SDK_FILE := $(notdir $(ARM_SDK_URL))
# order-only prereq on directory existance:
arm_sdk_install: | $(DL_DIR) $(TOOLS_DIR)
arm_sdk_install: arm_sdk_clean
ifneq ($(OSFAMILY), windows)
        # download the source only if it's newer than what we already have
	$(V1) wget --no-check-certificate -N -P "$(DL_DIR)" "$(ARM_SDK_URL)"

        # binary only release so just extract it
	$(V1) tar -C $(TOOLS_DIR) -xjf "$(DL_DIR)/$(ARM_SDK_FILE)"
else
	$(V1) curl -L -k -o "$(DL_DIR)/$(ARM_SDK_FILE)" "$(ARM_SDK_URL)"
	$(V1) unzip -q -d $(ARM_SDK_DIR) "$(DL_DIR)/$(ARM_SDK_FILE)"
endif

.PHONY: arm_sdk_clean
arm_sdk_clean:
	$(V1) [ ! -d "$(ARM_SDK_DIR)" ] || $(RM) -r $(ARM_SDK_DIR)

.PHONY: ftd2xx_install

FTD2XX_DIR := $(DL_DIR)/ftd2xx

ftd2xx_install: | $(DL_DIR)
ftd2xx_install: FTD2XX_URL  := http://www.ftdichip.com/Drivers/CDM/Beta/CDM20817.zip
ftd2xx_install: FTD2XX_FILE := CDM20817.zip
ftd2xx_install: ftd2xx_clean
        # download the file only if it's newer than what we already have
	$(V0) @echo " DOWNLOAD     $(FTD2XX_URL)"
	$(V1) wget -q -N -P "$(DL_DIR)" "$(FTD2XX_URL)"

        # extract the source
	$(V0) @echo " EXTRACT      $(FTD2XX_FILE) -> $(FTD2XX_DIR)"
	$(V1) mkdir -p "$(FTD2XX_DIR)"
	$(V1) unzip -q -d "$(FTD2XX_DIR)" "$(DL_DIR)/$(FTD2XX_FILE)"

.PHONY: ftd2xx_clean
ftd2xx_clean:
	$(V0) @echo " CLEAN        $(FTD2XX_DIR)"
	$(V1) [ ! -d "$(FTD2XX_DIR)" ] || $(RM) -r "$(FTD2XX_DIR)"

.PHONY: ftd2xx_install

LIBUSB_WIN_DIR := $(DL_DIR)/libusb-win32-bin-1.2.6.0

libusb_win_install: | $(DL_DIR)
libusb_win_install: LIBUSB_WIN_URL  := http://sourceforge.net/projects/libusb-win32/files/libusb-win32-releases/1.2.6.0/libusb-win32-bin-1.2.6.0.zip/download
libusb_win_install: LIBUSB_WIN_FILE := libusb-win32-bin-1.2.6.0.zip
libusb_win_install: libusb_win_clean
        # download the file only if it's newer than what we already have
	$(V0) @echo " DOWNLOAD     $(LIBUSB_WIN_URL)"
	$(V1) wget -q -N -P "$(DL_DIR)" --trust-server-name "$(LIBUSB_WIN_URL)"

        # extract the source
	$(V0) @echo " EXTRACT      $(LIBUSB_WIN_FILE) -> $(LIBUSB_WIN_DIR)"
	$(V1) mkdir -p "$(LIBUSB_WIN_DIR)"
	$(V1) unzip -q -d "$(DL_DIR)" "$(DL_DIR)/$(LIBUSB_WIN_FILE)"

        # fixup .h file needed by openocd build
	$(V0) @echo " FIXUP        $(LIBUSB_WIN_DIR)"
	$(V1) ln -s "$(LIBUSB_WIN_DIR)/include/lusb0_usb.h" "$(LIBUSB_WIN_DIR)/include/usb.h"

.PHONY: libusb_win_clean
libusb_win_clean:
	$(V0) @echo " CLEAN        $(LIBUSB_WIN_DIR)"
	$(V1) [ ! -d "$(LIBUSB_WIN_DIR)" ] || $(RM) -r "$(LIBUSB_WIN_DIR)"

.PHONY: openocd_win_install

openocd_win_install: | $(DL_DIR) $(TOOLS_DIR)
openocd_win_install: OPENOCD_URL  := git://git.code.sf.net/p/openocd/code
openocd_win_install: OPENOCD_REV  := cf1418e9a85013bbf8dbcc2d2e9985695993d9f4
openocd_win_install: OPENOCD_OPTIONS :=

ifeq ($(OPENOCD_FTDI), yes)
openocd_win_install: OPENOCD_OPTIONS := $(OPENOCD_OPTIONS) --enable-ft2232_ftd2xx --with-ftd2xx-win32-zipdir=$(FTD2XX_DIR)
endif

openocd_win_install: openocd_win_clean libusb_win_install ftd2xx_install
        # download the source
	$(V0) @echo " DOWNLOAD     $(OPENOCD_URL) @ $(OPENOCD_REV)"
	$(V1) [ ! -d "$(OPENOCD_BUILD_DIR)" ] || $(RM) -rf "$(OPENOCD_BUILD_DIR)"
	$(V1) mkdir -p "$(OPENOCD_BUILD_DIR)"
	$(V1) git clone --no-checkout $(OPENOCD_URL) "$(DL_DIR)/openocd-build"
	$(V1) ( \
	  cd $(OPENOCD_BUILD_DIR) ; \
	  git checkout -q $(OPENOCD_REV) ; \
	)

        # apply patches
	$(V0) @echo " PATCH        $(OPENOCD_BUILD_DIR)"
	$(V1) ( \
	  cd $(OPENOCD_BUILD_DIR) ; \
	  git apply < $(ROOT_DIR)/flight/Project/OpenOCD/0003-freertos-cm4f-fpu-support.patch ; \
	  git apply < $(ROOT_DIR)/flight/Project/OpenOCD/0004-st-icdi-disable.patch ; \
	)

        # build and install
	$(V0) @echo " BUILD        $(OPENOCD_WIN_DIR)"
	$(V1) mkdir -p "$(OPENOCD_WIN_DIR)"
	$(V1) ( \
	  cd $(OPENOCD_BUILD_DIR) ; \
	  ./bootstrap ; \
	  ./configure --enable-maintainer-mode --prefix="$(OPENOCD_WIN_DIR)" \
		--build=i686-pc-linux-gnu --host=i586-mingw32msvc \
		CPPFLAGS=-I$(LIBUSB_WIN_DIR)/include \
		LDFLAGS=-L$(LIBUSB_WIN_DIR)/lib/gcc \
		$(OPENOCD_OPTIONS) \
		--disable-werror \
		--enable-stlink ; \
	  $(MAKE) ; \
	  $(MAKE) install ; \
	)

        # delete the extracted source when we're done
	$(V1) [ ! -d "$(OPENOCD_BUILD_DIR)" ] || $(RM) -rf "$(OPENOCD_BUILD_DIR)"

.PHONY: openocd_win_clean
openocd_win_clean:
	$(V0) @echo " CLEAN        $(OPENOCD_WIN_DIR)"
	$(V1) [ ! -d "$(OPENOCD_WIN_DIR)" ] || $(RM) -r "$(OPENOCD_WIN_DIR)"

# Set up openocd tools
OPENOCD_DIR       := $(TOOLS_DIR)/openocd
OPENOCD_WIN_DIR   := $(TOOLS_DIR)/openocd_win
OPENOCD_BUILD_DIR := $(DL_DIR)/openocd-build

.PHONY: openocd_install

openocd_install: | $(DL_DIR) $(TOOLS_DIR)
openocd_install: OPENOCD_URL     := git://git.code.sf.net/p/openocd/code
openocd_install: OPENOCD_TAG     := v0.9.0
openocd_install: OPENOCD_OPTIONS := --enable-maintainer-mode --prefix="$(OPENOCD_DIR)" --enable-buspirate --enable-stlink

ifeq ($(OPENOCD_FTDI), yes)
openocd_install: OPENOCD_OPTIONS := $(OPENOCD_OPTIONS) --enable-ftdi
endif

ifeq ($(UNAME), Darwin)
openocd_install: OPENOCD_OPTIONS := $(OPENOCD_OPTIONS) --disable-option-checking
endif

openocd_install: openocd_clean
        # download the source
	$(V0) @echo " DOWNLOAD     $(OPENOCD_URL) @ $(OPENOCD_TAG)"
	$(V1) [ ! -d "$(OPENOCD_BUILD_DIR)" ] || $(RM) -rf "$(OPENOCD_BUILD_DIR)"
	$(V1) mkdir -p "$(OPENOCD_BUILD_DIR)"
	$(V1) git clone --no-checkout $(OPENOCD_URL) "$(OPENOCD_BUILD_DIR)"
	$(V1) ( \
	  cd $(OPENOCD_BUILD_DIR) ; \
	  git checkout -q tags/$(OPENOCD_TAG) ; \
	)

        # build and install
	$(V0) @echo " BUILD        $(OPENOCD_DIR)"
	$(V1) mkdir -p "$(OPENOCD_DIR)"
	$(V1) ( \
	  cd $(OPENOCD_BUILD_DIR) ; \
	  ./bootstrap ; \
	  ./configure  $(OPENOCD_OPTIONS) ; \
	  $(MAKE) ; \
	  $(MAKE) install ; \
	)

        # delete the extracted source when we're done
	$(V1) [ ! -d "$(OPENOCD_BUILD_DIR)" ] || $(RM) -rf "$(OPENOCD_BUILD_DIR)"

.PHONY: openocd_clean
openocd_clean:
	$(V0) @echo " CLEAN        $(OPENOCD_DIR)"
	$(V1) [ ! -d "$(OPENOCD_DIR)" ] || $(RM) -r "$(OPENOCD_DIR)"

STM32FLASH_DIR := $(TOOLS_DIR)/stm32flash

.PHONY: stm32flash_install
stm32flash_install: STM32FLASH_URL := http://stm32flash.googlecode.com/svn/trunk
stm32flash_install: STM32FLASH_REV := 61
stm32flash_install: stm32flash_clean
        # download the source
	$(V0) @echo " DOWNLOAD     $(STM32FLASH_URL) @ r$(STM32FLASH_REV)"
	$(V1) svn export -q -r "$(STM32FLASH_REV)" "$(STM32FLASH_URL)" "$(STM32FLASH_DIR)"

        # build
	$(V0) @echo " BUILD        $(STM32FLASH_DIR)"
	$(V1) $(MAKE) --silent -C $(STM32FLASH_DIR) all

.PHONY: stm32flash_clean
stm32flash_clean:
	$(V0) @echo " CLEAN        $(STM32FLASH_DIR)"
	$(V1) [ ! -d "$(STM32FLASH_DIR)" ] || $(RM) -r "$(STM32FLASH_DIR)"

DFUUTIL_DIR := $(TOOLS_DIR)/dfu-util

.PHONY: dfuutil_install
dfuutil_install: DFUUTIL_URL  := http://dfu-util.sourceforge.net/releases/dfu-util-0.8.tar.gz
dfuutil_install: DFUUTIL_FILE := $(notdir $(DFUUTIL_URL))
dfuutil_install: | $(DL_DIR) $(TOOLS_DIR)
dfuutil_install: dfuutil_clean
        # download the source
	$(V0) @echo " DOWNLOAD     $(DFUUTIL_URL)"
	$(V1) wget -N -P "$(DL_DIR)" "$(DFUUTIL_URL)"

        # extract the source
	$(V0) @echo " EXTRACT      $(DFUUTIL_FILE)"
	$(V1) [ ! -d "$(DL_DIR)/dfuutil-build" ] || $(RM) -r "$(DL_DIR)/dfuutil-build"
	$(V1) mkdir -p "$(DL_DIR)/dfuutil-build"
	$(V1) tar -C $(DL_DIR)/dfuutil-build -xf "$(DL_DIR)/$(DFUUTIL_FILE)"

        # build
	$(V0) @echo " BUILD        $(DFUUTIL_DIR)"
	$(V1) mkdir -p "$(DFUUTIL_DIR)"
	$(V1) ( \
	  cd $(DL_DIR)/dfuutil-build/dfu-util-0.8 ; \
	  ./configure --prefix="$(DFUUTIL_DIR)" ; \
	  $(MAKE) ; \
	  $(MAKE) install ; \
	)

.PHONY: dfuutil_clean
dfuutil_clean:
	$(V0) @echo " CLEAN        $(DFUUTIL_DIR)"
	$(V1) [ ! -d "$(DFUUTIL_DIR)" ] || $(RM) -r "$(DFUUTIL_DIR)"

# see http://developer.android.com/sdk/ for latest versions
.PHONY: android_sdk_install
android_sdk_install: ANDROID_SDK_FILE := $(notdir $(ANDROID_SDK_URL))
# order-only prereq on directory existance:
android_sdk_install: | $(DL_DIR) $(TOOLS_DIR)
android_sdk_install: android_sdk_clean
        # download the source only if it's newer than what we already have
	$(V0) @echo " DOWNLOAD     $(ANDROID_SDK_URL)"
	$(V1) wget --no-check-certificate -N -P "$(DL_DIR)" "$(ANDROID_SDK_URL)"

        # binary only release so just extract it
	$(V0) @echo " EXTRACT      $(ANDROID_SDK_FILE)"
	$(V1) tar -C $(TOOLS_DIR) -xf "$(DL_DIR)/$(ANDROID_SDK_FILE)"

.PHONY: android_sdk_clean
android_sdk_clean:
	$(V0) @echo " CLEAN        $(ANDROID_SDK_DIR)"
	$(V1) [ ! -d "$(ANDROID_SDK_DIR)" ] || $(RM) -r $(ANDROID_SDK_DIR)

.PHONY: android_sdk_update
android_sdk_update:
	$(V0) @echo " UPDATE       $(ANDROID_SDK_DIR)"
	$(ANDROID_SDK_DIR)/tools/android update sdk --no-ui --all -t platform-tools,build-tools-20.0.0,android-19,addon-google_apis-google-19

# Set up Google Test (gtest) tools
GTEST_DIR       := $(TOOLS_DIR)/googletest-release-1.8.0

.PHONY: gtest_install
gtest_install: | $(DL_DIR) $(TOOLS_DIR)
gtest_install: GTEST_URL  := https://github.com/google/googletest/archive/release-1.8.0.zip
gtest_install: GTEST_FILE := $(notdir $(GTEST_URL))
gtest_install: gtest_clean
        # download the file unconditionally since google code gives back 404
        # for HTTP HEAD requests which are used when using the wget -N option
	$(V1) [ ! -f "$(DL_DIR)/$(GTEST_FILE)" ] || $(RM) -f "$(DL_DIR)/$(GTEST_FILE)"
	$(V1) wget -P "$(DL_DIR)" "$(GTEST_URL)"

        # extract the source
	$(V1) [ ! -d "$(GTEST_DIR)" ] || $(RM) -rf "$(GTEST_DIR)"
	$(V1) mkdir -p "$(GTEST_DIR)"
	$(V1) unzip -q -d "$(TOOLS_DIR)" "$(DL_DIR)/$(GTEST_FILE)"
	$(V1) mv $(GTEST_DIR)/googletest/* $(GTEST_DIR)

.PHONY: gtest_clean
gtest_clean:
	$(V0) @echo " CLEAN        $(GTEST_DIR)"
	$(V1) [ ! -d "$(GTEST_DIR)" ] || $(RM) -rf "$(GTEST_DIR)"

# Set up uncrustify tools
UNCRUSTIFY_DIR := $(TOOLS_DIR)/uncrustify-0.61
UNCRUSTIFY_BUILD_DIR := $(DL_DIR)/uncrustify

.PHONY: uncrustify_install
uncrustify_install: | $(DL_DIR) $(TOOLS_DIR)
uncrustify_install: UNCRUSTIFY_URL := http://downloads.sourceforge.net/project/uncrustify/uncrustify/uncrustify-0.61/uncrustify-0.61.tar.gz
uncrustify_install: UNCRUSTIFY_FILE := uncrustify-0.61.tar.gz
uncrustify_install: UNCRUSTIFY_OPTIONS := prefix=$(UNCRUSTIFY_DIR)
uncrustify_install: uncrustify_clean
ifneq ($(OSFAMILY), windows)
	$(V0) @echo " DOWNLOAD     $(UNCRUSTIFY_URL)"
	$(V1) wget --no-check-certificate -N -P "$(DL_DIR)" "$(UNCRUSTIFY_URL)"
else
	$(V1) curl -L -k -o "$(DL_DIR)/$(UNCRUSTIFY_FILE)" "$(UNCRUSTIFY_URL)"
endif
        # extract the src
	$(V0) @echo " EXTRACT      $(UNCRUSTIFY_FILE)"
	$(V1) tar -C $(TOOLS_DIR) -xf "$(DL_DIR)/$(UNCRUSTIFY_FILE)"

	$(V0) @echo " BUILD        $(UNCRUSTIFY_DIR)"
	$(V1) ( \
	  cd $(UNCRUSTIFY_DIR) ; \
	  ./configure --prefix="$(UNCRUSTIFY_DIR)" ; \
	  $(MAKE) ; \
	  $(MAKE) install ; \
	)
	      # delete the extracted source when we're done
	$(V1) [ ! -d "$(UNCRUSTIFY_BUILD_DIR)" ] || $(RM) -r "$(UNCRUSTIFY_BUILD_DIR)"

.PHONY: uncrustify_clean
uncrustify_clean:
	$(V0) @echo " CLEAN        $(UNCRUSTIFY_DIR)"
	$(V1) [ ! -d "$(UNCRUSTIFY_DIR)" ] || $(RM) -r "$(UNCRUSTIFY_DIR)"
	$(V0) @echo " CLEAN        $(UNCRUSTIFY_BUILD_DIR)"
	$(V1) [ ! -d "$(UNCRUSTIFY_BUILD_DIR)" ] || $(RM) -r "$(UNCRUSTIFY_BUILD_DIR)"

# Set up libkml

.PHONY: libkml_install
libkml_install: | $(DL_DIR) $(TOOLS_DIR)
libkml_install: LIBKML_URL := https://github.com/kubark42/libkml.git
libkml_install: LIBKML_REV  := e4e1c525363b8b1bbe5d6683827a07e7c252b815
libkml_install: LIBKML_INSTALL_DIR := $(TOOLS_DIR)/libkml
libkml_install: LIBKML_BUILD_DIR := $(DL_DIR)/libkml-build
libkml_install: libkml_clean
        # download the source
	$(V0) @echo " DOWNLOAD     $(LIBKML_URL) @ $(LIBKML_REV)"
	$(V1) [ ! -d "$(LIBKML_BUILD_DIR)" ] || $(RM) -rf "$(LIBKML_BUILD_DIR)"
	$(V1) mkdir -p "$(LIBKML_BUILD_DIR)"
	$(V1) git clone --no-checkout $(LIBKML_URL) "$(LIBKML_BUILD_DIR)"
	$(V1) ( \
	  cd $(LIBKML_BUILD_DIR) ; \
	  git checkout -q $(LIBKML_REV) ; \
	)

        # build and install
	$(V0) @echo " BUILD        $(LIBKML_INSTALL_DIR)"
	$(V1) mkdir -p "$(LIBKML_BUILD_DIR)/build"
	$(V1) ( \
	  cd $(LIBKML_BUILD_DIR) ; \
	  ./autogen.sh ; \
	  cd $(LIBKML_BUILD_DIR)/build ; \
	  ../configure --prefix="$(LIBKML_INSTALL_DIR)"; \
	  $(MAKE) ; \
	  $(MAKE) install ; \
	)

        # delete the extracted source when we're done
	$(V1) [ ! -d "$(LIBKML_BUILD_DIR)" ] || $(RM) -rf "$(LIBKML_BUILD_DIR)"

.PHONY: libkml_clean
libkml_clean:
	$(V0) @echo " CLEAN        $(LIBKML_INSTALL_DIR)"
	$(V1) [ ! -d "$(LIBKML_INSTALL_DIR)" ] || $(RM) -rf "$(LIBKML_INSTALL_DIR)"
	$(V0) @echo " CLEAN        $(LIBKML_BUILD_DIR)"
	$(V1) [ ! -d "$(LIBKML_BUILD_DIR)" ] || $(RM) -rf "$(LIBKML_BUILD_DIR)"

# ZIP download URL
zip_install: ZIP_URL  := http://pkgs.fedoraproject.org/repo/pkgs/zip/zip30.tar.gz/7b74551e63f8ee6aab6fbc86676c0d37/zip30.tar.gz

zip_install: ZIP_FILE := $(notdir $(ZIP_URL))

ZIP_DIR = $(TOOLS_DIR)/zip30

# order-only prereq on directory existance:
zip_install : | $(DL_DIR) $(TOOLS_DIR)
zip_install: zip_clean
	$(V1) curl -L -k -o "$(DL_DIR)/$(ZIP_FILE)" "$(ZIP_URL)"
	$(V1) tar --force-local -C $(TOOLS_DIR) -xzf "$(DL_DIR)/$(ZIP_FILE)"
ifneq ($(OSFAMILY), windows)
	$(V1) cd "$(ZIP_DIR)" && $(MAKE) -f unix/Makefile generic_gcc
else
	$(V1) cd "$(ZIP_DIR)" && $(MAKE) -f win32/makefile.gcc
endif

.PHONY: zip_clean
zip_clean:
	$(V1) [ ! -d "$(ZIP_DIR)" ] || $(RM) -rf $(ZIP_DIR)


# Google breakpad
# Must match ground/tools.pri
BREAKPAD_REV := 20170922
BREAKPAD_REPO := https://github.com/d-ronin/breakpad.git
BREAKPAD_DIR := $(TOOLS_DIR)/breakpad/$(BREAKPAD_REV)
BREAKPAD_BUILD_DIR := $(DL_DIR)/breakpad

.PHONY: breakpad_install breakpad_clean tools_required_breakpad

breakpad_install: | tools_required_qbs
breakpad_install: | $(DL_DIR) $(TOOLS_DIR)
breakpad_install: | breakpad_clean
	$(V0) @echo " DOWNLOAD     $(BREAKPAD_REPO) @ $(BREAKPAD_REV)"
	$(V1) [ ! -d "$(BREAKPAD_BUILD_DIR)" ] || $(RM) -rf "$(BREAKPAD_BUILD_DIR)"
	$(V1) mkdir -p "$(BREAKPAD_BUILD_DIR)"
	$(V1) ( \
	  cd $(BREAKPAD_BUILD_DIR) ; \
	  git init -q ; \
	  git remote add upstream "$(BREAKPAD_REPO)" ; \
	  git fetch -q --tags --depth=1 --recurse-submodules upstream $(BREAKPAD_REV) ; \
	  git checkout -q $(BREAKPAD_REV) ; \
	  git submodule -q update --init ; \
	)

	$(V0) @echo " BUILDING     $(BREAKPAD_REPO) @ $(BREAKPAD_REV)"
	$(V1) ( \
	  cd $(BREAKPAD_BUILD_DIR) ; \
	  $(QBS) install --settings-dir "$(QBS_SETTINGS_DIR)" --install-root $(BREAKPAD_DIR) profile:$(QBS_PROFILE) release ; \
	  $(QBS) install --settings-dir "$(QBS_SETTINGS_DIR)" --install-root $(BREAKPAD_DIR) -p breakpad_client profile:$(QBS_PROFILE) debug ; \
	)

breakpad_clean:
	$(V0) @echo " CLEAN        $(BREAKPAD_DIR)"
	$(V1) [ ! -d "$(BREAKPAD_DIR)" ] || $(RM) -rf $(BREAKPAD_DIR)

tools_required_breakpad:
ifeq ($(wildcard $(BREAKPAD_DIR)/*),)
	$(error "Breakpad not found, please run `make breakpad_install`")
else
	$(info "Breakpad found in $(BREAKPAD_DIR)")
endif


##############################
#
# Set up paths to tools
#
##############################

QBS_SETTINGS_DIR := $(TOOLS_DIR)/qbs
ifeq ($(shell [ -d "$(QT_SDK_DIR)" ] && echo "exists"), exists)
  QMAKE = $(QT_SDK_QMAKE_PATH)
  QBS = $(QT_SDK_QBS_PATH)
else
  # not installed, hope it's in the path...
  QMAKE = qmake
  QBS = qbs
endif

.PHONY: tools_required_qbs

tools_required_qbs: | tools_required_qt
ifeq (,$(shell $(QBS) config --settings-dir "$(QBS_SETTINGS_DIR)" --list profiles))
# empty profile config, detect toolchains
	$(V1) $(QBS) setup-toolchains --settings-dir "$(QBS_SETTINGS_DIR)" --detect
else ifeq (,$(shell $(QBS) config --settings-dir "$(QBS_SETTINGS_DIR)" --list | grep "profiles\.$(QBS_PROFILE)\.qbs"))
# profile we want is missing. let user know so they can decide if they want to overwrite their config (needed since we don't own Qt install on Windows)
	$(error "QBS profile '$(QBS_PROFILE)' not found. Try running '$(QBS) setup-toolchains --settings-dir "$(QBS_SETTINGS_DIR)" --detect'")
endif

ifeq ($(shell [ -d "$(ARM_SDK_DIR)" ] && echo "exists"), exists)
  ARM_SDK_PREFIX := $(ARM_SDK_DIR)/bin/arm-none-eabi-
else
  ifndef IGNORE_MISSING_TOOLCHAIN
    ifeq (,$(findstring _install,$(MAKECMDGOALS)))
      $(error **WARNING** ARM-SDK not in $(ARM_SDK_DIR)  Please run 'make arm_sdk_install')
    endif
  endif
  # not installed, hope it's in the path...
  ARM_SDK_PREFIX ?= arm-none-eabi-
endif

ifeq ($(shell [ -d "$(ZIP_DIR)" ] && echo "exists"), exists)
  export ZIPBIN := $(ZIP_DIR)/zip
else
  export ZIPBIN := zip
endif

ifeq ($(shell [ -d "$(OPENOCD_DIR)" ] && echo "exists"), exists)
  OPENOCD := $(OPENOCD_DIR)/bin/openocd
else
  # not installed, hope it's in the path...
  OPENOCD ?= openocd
endif

ifeq ($(shell [ -d "$(ANDROID_SDK_DIR)" ] && echo "exists"), exists)
  ANDROID     := $(ANDROID_SDK_DIR)/tools/android
  ANDROID_DX  := $(ANDROID_SDK_DIR)/build-tools/20.0.0/dx
  ANDROID_ADB := $(ANDROID_SDK_DIR)/platform-tools/adb
else
  # not installed, hope it's in the path...
  ANDROID     ?= android
  ANDROID_DX  ?= dx
  ANDROID_ADB ?= adb
endif

ifeq ($(shell [ -d "$(ASTYLE_DIR)" ] && echo "exists"), exists)
  ASTYLE := $(ASTYLE_DIR)/bin/astyle
else
  # not installed, hope it's in the path...
  ASTYLE ?= astyle
endif

ifeq ($(shell [ -d "$(UNCRUSTIFY_DIR)" ] && echo "exists"), exists)
  UNCRUSTIFY := $(UNCRUSTIFY_DIR)/bin/uncrustify
else
  # not installed, hope it's in the path...
  UNCRUSTIFY ?= uncrustify
endif

ifeq ($(shell [ -d "$(BREAKPAD_DIR)" ] && echo "exists"), exists)
  DUMP_SYMBOLS_TOOL := $(BREAKPAD_DIR)/bin/dump_syms
else
  ifndef IGNORE_MISSING_TOOLCHAIN
    ifneq (,$(filter package%, $(MAKECMDGOALS)))
      $(error **ERROR** breakpad not in $(BREAKPAD_DIR)! Please run 'make breakpad_install')
    endif
  endif
  DUMP_SYMBOLS_TOOL := dump_syms
endif

.PHONY: openssl_install

# OPENSSL download URL
ifdef WINDOWS
  openssl_install: OPENSSL_URL  = https://slproweb.com$(shell \
          curl -s https://slproweb.com/products/Win32OpenSSL.html \
          | grep -P 'Win32OpenSSL-1_0_[0-9]*[A-Za-z].exe' \
          | head -1 \
          | sed 's|.*\(/download/Win32OpenSSL.*\.exe\).*|\1|g'\
  )

openssl_install: OPENSSL_FILE = $(notdir $(OPENSSL_URL))
OPENSSL_DIR = $(TOOLS_DIR)/win32openssl
# order-only prereq on directory existance:
openssl_install : | $(DL_DIR) $(TOOLS_DIR)
openssl_install: openssl_clean
	$(V1) curl -L -k -o "$(DL_DIR)/$(OPENSSL_FILE)" "$(OPENSSL_URL)"
	$(V1) ./downloads/$(OPENSSL_FILE) /DIR=$(OPENSSL_DIR) /silent
else
openssl_install:
	$(V1) $(error THIS IS A WINDOWS ONLY TARGET)
endif

.PHONY: openssl_clean
openssl_clean:
	$(V1) [ ! -d "$(OPENSSL_DIR)" ] || $(RM) -rf $(OPENSSL_DIR)

.PHONY: sdl_install

# SDL download URL
SDL_DIR  := $(TOOLS_DIR)/SDL.framework
SDL_URL  := https://www.libsdl.org/release/SDL-1.2.15.dmg
SDL_FILE := SDL-1.2.15.dmg

ifdef MACOSX
sdl_install:
	$(V0) @echo " DOWNLOAD $(SDL_URL) "
	$(V1) curl -L -k -o "$(DL_DIR)/$(SDL_FILE)" "$(SDL_URL)"
	$(V1) hdiutil attach $(DL_DIR)/$(SDL_FILE);
	$(V1) cp -r /Volumes/SDL/SDL.framework $(TOOLS_DIR);
	$(V1) hdiutil detach /Volumes/SDL;
	$(V1) rm $(DL_DIR)/$(SDL_FILE);
endif
