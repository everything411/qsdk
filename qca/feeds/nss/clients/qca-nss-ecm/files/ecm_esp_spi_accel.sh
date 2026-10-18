#!/bin/sh
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC

#
# usage:
# enable ESP SPI acceleration configuration
#       ./lib/ecm_esp_spi_accel.sh start
# disable ESP SPI acceleration configuration
#       ./lib/ecm_esp_spi_accel.sh stop
# show ESP SPI acceleration configuration status
#       ./lib/ecm_esp_spi_accel.sh status
#

write_if_exists() {
        local value="$1"
        local path="$2"

        [ -e "$path" ] && echo "$value" > "$path"
}

print_if_exists() {
        local name="$1"
        local path="$2"

        [ -e "$path" ] && echo "$name=$(cat "$path")"
}

set_sysctls() {
        local value="$1"

        write_if_exists "$value" /proc/sys/net/netfilter/nf_conntrack_esp_enabled
        write_if_exists "$value" /proc/sys/net/ecm/esp_spi_passthrough_enable
        write_if_exists "$value" /sys/sfe/esp_spi_passthrough_feature
        write_if_exists "$value" /proc/sys/ppe/ppe_drv/ppe_drv_ipsec_passth_en
}

start() {
        set_sysctls 1
        echo "ecm_esp_spi_accel: ESP SPI acceleration enabled"
}

stop() {
        set_sysctls 0
        echo "ecm_esp_spi_accel: ESP SPI acceleration disabled"
}

status() {
        echo "ecm_esp_spi_accel: current status"
        print_if_exists nf_conntrack_esp_enabled /proc/sys/net/netfilter/nf_conntrack_esp_enabled
        print_if_exists esp_spi_passthrough_enable /proc/sys/net/ecm/esp_spi_passthrough_enable
        print_if_exists esp_spi_passthrough_feature /sys/sfe/esp_spi_passthrough_feature
        print_if_exists ppe_drv_ipsec_passth_en /proc/sys/ppe/ppe_drv/ppe_drv_ipsec_passth_en
}

usage() {
        echo "Usage: $0 {start|stop|status}"
        exit 1
}

case "$1" in
start)
        start
;;
stop)
        stop
;;
status)
        status
;;
*)
        usage
;;
esac
