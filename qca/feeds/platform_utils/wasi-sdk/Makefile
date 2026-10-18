include $(TOPDIR)/rules.mk

PKG_NAME:=wasi-sdk
PKG_VERSION:=24
PKG_RELEASE:=1

PKG_SOURCE:=wasi-sdk-$(PKG_VERSION).0-x86_64-linux.tar.gz
PKG_SOURCE_URL:=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/
PKG_HASH:=c6c38aab56e5de88adf6c1ebc9c3ae8da72f88ec2b656fb024eda8d4167a0bc5

PKG_STAG_FOLDER:=wasi-sdk-$(PKG_VERSION).0-x86_64-linux
PKG_STAG_DIR:=$(STAGING_DIR_HOST)/$(PKG_STAG_FOLDER)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	MAINTAINER:=WASI
	TITLE:=WASI SDK
	URL:=https://github.com/WebAssembly/wasi-sdk
	TITLE:=Toolchain for WASM Microruntime 
	LICENSE:=Apache License v2.0 with LLVM Exceptions
	DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
endef

define Package/$(PKG_NAME)/description
	WebAssembly System Interface (WASI) SDK to build WAMR container
endef

define Host/Prepare
	tar -xvf $(DL_DIR)/$(PKG_SOURCE) -C $(STAGING_DIR_HOST)
endef

define Host/Compile
	# No compilation needed, just extraction
	@echo "Setting up environment variables on host..."
	export WASI_SDK_INSTALLTION_DIR=$(PKG_STAG_DIR)
	export CLANG="$(PKG_STAG_DIR)/bin/clang --sysroot=$(PKG_STAG_DIR)/share/wasi-sysroot"
	export CLANGXX="$(PKG_STAG_DIR)/bin/clang++ --sysroot=$(PKG_STAG_DIR)/share/wasi-sysroot"
endef


define Package/$(PKG_NAME)/install
	# Dummy install section to avoid warnings
endef

define Host/Install
	# Dummy install section to avoid warnings
endef

$(eval $(call BuildPackage,wasi-sdk))
$(eval $(call HostBuild))
