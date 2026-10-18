: '
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
'

#!/bin/bash

set -e

PKG_BUILD_DIR="$1"
BIN_DIR="$2"
PLATFORM="$3"
SUBTARGET="$4"
VARIANT="$5"
TEXT_BASE="$6"
TEXT_SIZE="$7"
ARCH="$8"
OBJCOPY="$9"
LD="${10}"
STRIP="${11}"
CP="${12}"

if [ -z "$PKG_BUILD_DIR" ] || [ -z "$BIN_DIR" ] || [ -z "$PLATFORM" ] || \
   [ -z "$SUBTARGET" ] || [ -z "$VARIANT" ] || [ -z "$TEXT_BASE" ] || \
   [ -z "$TEXT_SIZE" ] || [ -z "$ARCH" ] || [ -z "$OBJCOPY" ] || \
   [ -z "$LD" ] || [ -z "$STRIP" ] || [ -z "$CP" ]; then
	echo "ERROR: Missing required parameters"
	echo "Usage: $0 PKG_BUILD_DIR BIN_DIR PLATFORM SUBTARGET VARIANT TEXT_BASE TEXT_SIZE ARCH OBJCOPY LD STRIP CP"
	exit 1
fi

LD_SCRIPT_NAME="u-boot-${SUBTARGET}-custom.ld"
UBOOTDTB_DIR="${PKG_BUILD_DIR}/u-boot-dtb"

create_linker_script() {
	local ld_script_path="$1"
	cat > "${ld_script_path}" << EOF
MEMORY {
	DDR (rxw) : ORIGIN = ${TEXT_BASE}, LENGTH = ${TEXT_SIZE}
}
PHDRS {
	data PT_LOAD FLAGS(5);
}
ENTRY(_entry)
SECTIONS {
	. = ${TEXT_BASE};
	_entry = . ;
	.data : { *(.data) . = ALIGN(64);} > DDR :data
	_end = .;
}
EOF
}

process_fit_image() {
	echo "CONFIG_MULTI_DTB_FIT is present - processing single u-boot.bin with FIT image"

	if [ ! -f "${PKG_BUILD_DIR}/u-boot.bin" ]; then
		echo "ERROR: Required file u-boot.bin not found in ${PKG_BUILD_DIR}"
		exit 1
	fi

	if [ ! -f "${PKG_BUILD_DIR}/u-boot" ]; then
		echo "ERROR: Required file u-boot not found in ${PKG_BUILD_DIR}"
		exit 1
	fi

	LD_SCRIPT_PATH="${PKG_BUILD_DIR}/${LD_SCRIPT_NAME}"
	create_linker_script "${LD_SCRIPT_PATH}"

	${OBJCOPY} -I binary -O ${ARCH} \
		--change-addresses ${TEXT_BASE} \
		--set-start ${TEXT_BASE} \
		"${PKG_BUILD_DIR}/u-boot.bin" \
		"${PKG_BUILD_DIR}/u-boot.o"

	${LD} "${PKG_BUILD_DIR}/u-boot.o" \
		-T "${LD_SCRIPT_PATH}" \
		-o "${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot.elf"

	${CP} "${PKG_BUILD_DIR}/u-boot" \
		"${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-unstripped.elf"

	${CP} "${PKG_BUILD_DIR}/u-boot" \
		"${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-stripped.elf"
	${STRIP} "${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-stripped.elf"

	${CP} "${PKG_BUILD_DIR}/u-boot.bin" \
		"${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot.img"

	echo "Created: openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot.elf"
}

process_individual_dtbs() {
	echo "CONFIG_MULTI_DTB_FIT not present - processing individual DTBs"

	if [ "${SUBTARGET}" = "generic" ]; then
		if ls "${PKG_BUILD_DIR}/dts/upstream/src/arm64/qcom/${PLATFORM}"*.dtb 1> /dev/null 2>&1; then
			${CP} "${PKG_BUILD_DIR}/dts/upstream/src/arm64/qcom/${PLATFORM}"*.dtb "${UBOOTDTB_DIR}/"
			echo "Copied 64bit ${PLATFORM}*.dtb from dts/upstream/src/arm64/qcom/"
		else
			echo "No 64 bit ${PLATFORM}*.dtb files found in dts/upstream/src/arm64/qcom/"
		fi
	else
		if ls "${PKG_BUILD_DIR}/dts/upstream/src/arm/qcom/${PLATFORM}"*.dtb 1> /dev/null 2>&1; then
			${CP} "${PKG_BUILD_DIR}/dts/upstream/src/arm/qcom/${PLATFORM}"*.dtb "${UBOOTDTB_DIR}/"
			echo "Copied 32bit ${PLATFORM}*.dtb from dts/upstream/src/arm/qcom/"
		else
			echo "No 32bit ${PLATFORM}*.dtb files found in dts/upstream/src/arm/qcom/"
		fi
	fi

	if ls "${PKG_BUILD_DIR}/arch/arm/dts/${PLATFORM}"*.dtb 1> /dev/null 2>&1; then
		${CP} "${PKG_BUILD_DIR}/arch/arm/dts/${PLATFORM}"*.dtb "${UBOOTDTB_DIR}/"
		echo "Copied ${PLATFORM}*.dtb from arch/arm/dts/"
	else
		echo "No ${PLATFORM}*.dtb files found in arch/arm/dts/"
	fi

	if ! ls "${UBOOTDTB_DIR}"/*.dtb 1> /dev/null 2>&1; then
		echo "ERROR: No DTB files found in ${UBOOTDTB_DIR}"
		exit 1
	fi

	echo "Collected DTB files:"
	ls "${UBOOTDTB_DIR}"/*.dtb

	if [ ! -f "${PKG_BUILD_DIR}/u-boot-nodtb.bin" ]; then
		echo "ERROR: Required file u-boot-nodtb.bin not found in ${PKG_BUILD_DIR}"
		exit 1
	fi

	if [ ! -f "${PKG_BUILD_DIR}/u-boot" ]; then
		echo "ERROR: Required file u-boot not found in ${PKG_BUILD_DIR}"
		exit 1
	fi

	LD_SCRIPT_PATH="${PKG_BUILD_DIR}/${LD_SCRIPT_NAME}"
	create_linker_script "${LD_SCRIPT_PATH}"

	for dtb_file in "${UBOOTDTB_DIR}"/*.dtb; do
		[ -f "${dtb_file}" ] || continue

		dtb_name=$(basename "${dtb_file}" .dtb)
		dtb_name_stripped=$(echo "${dtb_name}" | sed "s/^${PLATFORM}-//")

		echo "=== Processing: ${dtb_name} ==="

		cat "${PKG_BUILD_DIR}/u-boot-nodtb.bin" \
			"${UBOOTDTB_DIR}/${dtb_name}.dtb" \
			> "${PKG_BUILD_DIR}/u-boot-${dtb_name}.bin"

		${OBJCOPY} -I binary -O ${ARCH} \
			--change-addresses ${TEXT_BASE} \
			--set-start ${TEXT_BASE} \
			"${PKG_BUILD_DIR}/u-boot-${dtb_name}.bin" \
			"${PKG_BUILD_DIR}/u-boot-${dtb_name}.o"

		${LD} "${PKG_BUILD_DIR}/u-boot-${dtb_name}.o" \
			-T "${LD_SCRIPT_PATH}" \
			-o "${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-${dtb_name_stripped}.elf"

		echo "Created: openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-${dtb_name_stripped}.elf"
	done

	${CP} "${PKG_BUILD_DIR}/u-boot" \
		"${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot-unstripped.elf"

	${CP} "${PKG_BUILD_DIR}/u-boot-${dtb_name}.bin" \
		"${BIN_DIR}/openwrt-${PLATFORM}-${SUBTARGET}-${VARIANT}-u-boot.img"
}

echo "=== U-Boot ELF Packing Script ==="
echo "Platform: ${PLATFORM}"
echo "Subtarget: ${SUBTARGET}"
echo "Variant: ${VARIANT}"
echo "Text Base: ${TEXT_BASE}"
echo "Text Size: ${TEXT_SIZE}"
echo "Architecture: ${ARCH}"

rm -rf "${UBOOTDTB_DIR}"
mkdir -p "${UBOOTDTB_DIR}"

if grep -q '^CONFIG_MULTI_DTB_FIT=y' "${PKG_BUILD_DIR}/.config"; then
	process_fit_image
else
	process_individual_dtbs
fi

echo "=== Packing complete ==="
