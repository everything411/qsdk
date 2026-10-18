include $(TOPDIR)/rules.mk

PKG_NAME    := mem-profile
PKG_VERSION := 1.0.0
PKG_RELEASE := 1

include $(INCLUDE_DIR)/package.mk

define Package/mem-profile
  SECTION  := base
  CATEGORY := Base system
  TITLE    := Memory profile boot-time validator
endef

define Package/mem-profile/description
  Validates the selected mem-profile at boot against physical MemTotal.
  Auto-corrects to the maximum profile allowed by the hardware and
  triggers a reboot if an incompatible profile is detected.
  Runs at START=00.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	# Shell-only package — nothing to compile
endef

define Package/mem-profile/install
	$(INSTALL_DIR)  $(1)/etc/init.d
	$(INSTALL_BIN)  ./files/etc/init.d/mem-profile  $(1)/etc/init.d/
	$(INSTALL_DIR)  $(1)/etc/board.d
	$(INSTALL_BIN)  ./files/etc/board.d/03_mem_profile  $(1)/etc/board.d/
	$(INSTALL_DIR)  $(1)/etc/config
	$(INSTALL_DATA) ./files/etc/config/mem-profile   $(1)/etc/config/mem-profile
endef


$(eval $(call BuildPackage,mem-profile))