cmake_minimum_required(VERSION 2.6)
project(OddLibs)

find_path (LIBNL_INCLUDE_DIR
  NAMES
  netlink/attr.h
  PATH_SUFFIXES
  libnl3
)

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/
	${LIBNL_INCLUDE_DIR}
)

set(LIBS nl-3 nl-genl-3)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Werror -DWLAN_ODD_HOST_PLUGIN")

ADD_LIBRARY(stats_lib SHARED stats_lib.c)
TARGET_LINK_LIBRARIES(stats_lib ${LIBS})

INSTALL(TARGETS stats_lib LIBRARY  DESTINATION /usr/lib)
