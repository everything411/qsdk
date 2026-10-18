include $(TOPDIR)/rules.mk

PKG_NAME:=libwamr
PKG_VERSION:=1.3.2
PKG_RELEASE:=1
PKG_SOURCE:=WAMR-$(PKG_VERSION).tar.gz

PKG_SOURCE_URL:=https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags
PKG_HASH:=58961ba387ed66ace2dd903597f1670a42b8154a409757ae6f06f43fe867a98c
CMAKE_INSTALL:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/wasm-micro-runtime-WAMR-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk
include $(INCLUDE_DIR)/host-build.mk

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:= Libraries
  MAINTAINER:=Bytecode Alliance
  TITLE:=Library implementing the WebAssembly Micro Runtime
  LICENSE:=Apache License v2.0 with LLVM Exceptions
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +libstdcpp
endef

define Package/$(PKG_NAME)/description
 WebAssembly Micro Runtime (WAMR) is a lightweight standalone WebAssembly (Wasm) runtime with small footprint, high performance and highly configurable features for applications cross from embedded, IoT, edge to Trusted Execution Environment (TEE), smart contract, cloud native and so on.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/libwamr
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/*.h $(1)/usr/include/libwamr/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libiwasm.so $(1)/usr/lib/
endef

define Package/${PKG_NAME}/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/product-mini/platforms/linux/iwasm $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libiwasm.so $(1)/usr/lib
endef

define Host/Prepare
	@echo "Building in Makefile..."
	cd $(PKG_BUILD_DIR)/wamr-compiler && \
	mkdir -p build && \
	./build_llvm.sh && cd build && cmake ..
endef

TARGET_CFLAGS += -DCMAKE_C_COMPILER=$(HOSTCC) -DCMAKE_CXX_COMPILER=$(HOSTCXX)

define Host/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/wamr-compiler/build CFLAGS="$(TARGET_CFLAGS)"
endef

define Host/Install
	$(INSTALL_DIR) $(STAGING_DIR_HOSTPKG)/bin/
	$(CP) $(PKG_BUILD_DIR)/wamr-compiler/build/wamrc $(STAGING_DIR_HOSTPKG)/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call HostBuild))
