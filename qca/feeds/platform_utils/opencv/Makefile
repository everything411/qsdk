#
# Copyright (C) 2013-2014 wrtnode.com
# Copyright (C) 2015-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

CPU_ARCH=armv7-a
TARGET_ARCH=armv7-a
ifneq ($(CONFIG_aarch64),)
TARGET_ARCH=armv8-a
CPU_ARCH=arm64-v8a
endif

PKG_NAME:=opencv

PKG_VERSION:=4.11.0
PKG_RELEASE:=1
PKG_SOURCE:=4.11.0.zip
PKG_HASH:=11dbd2c8d248fa97ac7d20f33c4bab8559ef32835d1c9274009150bb2cf5218a

PKG_SOURCE_URL:=https://github.com/opencv/opencv/archive/refs/tags/

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

CMAKE_BINARY_SUBDIR:=build

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/opencv/Default/description
 OpenCV (Open Source Computer Vision Library) is an open source computer
 vision and machine learning software library. OpenCV was built to provide
 a common infrastructure for computer vision applications and to accelerate
 the use of machine perception in the commercial products. Being a
 BSD-licensed product, OpenCV makes it easy for businesses to utilize
 and modify the code.
endef

define Package/opencv
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=OpenCV
  URL:=http://opencv.org/
  MAINTAINER:=WRTnode Team <pub@wrtnode.com>
  DEPENDS:=@TARGET_ipq95xx +libpthread +librt +libstdcpp +zlib +glib2 +ffmpeg
endef

PKG_INSTALL:=1

CMAKE_OPTIONS += -DBUILD_opencv_gpu:BOOL=OFF \
	-DBUILD_opencv_stitching:BOOL=OFF \
	-DBUILD_opencv_superres:BOOL=OFF -DBUILD_opencv_ts:BOOL=OFF \
	-DBUILD_opencv_highgui:BOOL=OFF \
	-DBUILD_opencv_videostab:BOOL=OFF \
	-DWITH_LIBV4L:BOOL=OFF \
	-DWITH_PNG:BOOL=OFF \
	-DWITH_TIFF:BOOL=OFF \
	-DWITH_FFMPEG:BOOL=ON \
	-DWITH_GSTREAMER:BOOL=ON \
	-DCMAKE_VERBOSE:BOOL=OFF \
	-DENABLE_PRECOMPILED_HEADERS=OFF\
	-DBUILD_JPEG:BOOL=ON

CMAKE_BINARY_DIR:=$(PKG_BUILD_DIR)/build

TARGET_CXXFLAGS+= -march=$(TARGET_ARCH) -std=c++11

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/opencv4 $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libopencv* $(1)/usr/lib/
endef

define Package/opencv/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libopencv* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,opencv))
