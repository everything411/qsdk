#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: ISC
#

#!/bin/bash

devtest_path=/local/mnt2/workspace/devtest/trigger_extra_plat/build_alder_nss64_premium/win.nss.1.0_hazelnut/qsdk

check_param() {
	qsdk_path=${2:-"${devtest_path}"}

	if [[ "$1" =~ ^(JHPPE|HMSPPE|MRPPE|APPE|MPPE|ALL|clean)$ ]]; then
		chip_type="$1"
		echo "----------Start compile ${chip_type} ${qsdk_path}--------"
	else
		echo -e "error: chip $1\nusage: ./build.sh <JHPPE|HMSPPE|MRPPE|APPE|MPPE|ALL> [QSDK_PATH]" &&  exit 1
	fi
}

build_ssdk_option() {
	CHIP=$1
	QSDK=$2
	OPTIONS=$3
	./make.sh clean
	./make.sh compile ${CHIP} ${QSDK} ${OPTIONS} -j31 || \
	{ echo -e "\n********compile ${CHIP} ${OPTIONS} failed********!!!\n" && exit 1; }
}

build_ssdk() {
	CHIP=$1
	QSDK=$2
	build_ssdk_option ${CHIP} ${QSDK} 
	build_ssdk_option ${CHIP} ${QSDK} "MINI_SSDK=enable LOWMEM_256=enable"
}

check_param $1 $2

if [[ "$chip_type" == "ALL" ]];then
	build_ssdk HMSPPE ${qsdk_path}
	build_ssdk MRPPE  ${qsdk_path}
	build_ssdk APPE   ${qsdk_path}
	build_ssdk MPPE   ${qsdk_path}
	build_ssdk JHPPE  ${qsdk_path}
elif [[ "$chip_type" == "clean" ]];then
	./make.sh clean
	rm ../build -rf
else
	build_ssdk $chip_type ${qsdk_path}
fi
