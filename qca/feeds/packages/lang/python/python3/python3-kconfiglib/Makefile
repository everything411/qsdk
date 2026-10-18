include $(TOPDIR)/rules.mk

PKG_NAME:=python3-kconfiglib
PKG_VERSION:=14.1.0
PKG_RELEASE:=1

PYPI_SOURCE_NAME:=kconfiglib

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/ulfalizer/Kconfiglib
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=061e71f7d78cb057762d88de088055361863deff
PKG_HASH := 48b933e6390080f456f7c04530e329b75f7311c00eca3baea1706e15fa054ad0
PKG_SOURCE_SUBMODULES:=skip
HOST_BUILD_DEPENDS:=python3/host python-build/host python-installer/host python-wheel/host

PKG_LICENSE:=ISC

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk
include $(TOPDIR)/feeds/packages/lang/python/python3-package.mk
include $(TOPDIR)/feeds/packages/lang/python/python3-host-build.mk

define Package/python3-kconfiglib
  SECTION:=lang
  CATEGORY:=Languages
  SUBMENU:=Python
  TITLE:=Kconfiglib
  URL:=https://github.com/ulfalizer/Kconfiglib
  VARIANT:=python3
endef

define Package/python3-kconfiglib/description
	Kconfiglib is a Kconfig implementation in Python 2/3.
endef

$(eval $(call Py3Package,python3-kconfiglib))
$(eval $(call BuildPackage,python3-kconfiglib))
$(eval $(call HostBuild))
