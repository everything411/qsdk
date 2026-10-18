# Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#!/bin/bash

################################################################################
#               input parameters
################################################################################
param1_action=$1			#compile/clean/test
param2_profile=$2 			#APPE/HPPE/CPPE/MPPE/MRPPE or profile_file
param3_qsdk=$3 				#qsdk location
param4_option=$4 			#"MINI_SSDK=enable LOWMEM_256=enable HNAT_FEATURE=enable RFS_FEATURE=enable"
paramlast_flags=${!#} 		#-j31

################################################################################
#               local parameters
################################################################################
prepare_compile() {
	export SSDK_PATH=$(pwd)/../
	if [[ -n "$param3_qsdk" && "$param3_qsdk" != -j* && 
			"$param3_qsdk" != *MINI_SSDK* && \
			"$param3_qsdk" != *LOWMEM_256* && \
			"$param3_qsdk" != *HNAT_FEATURE* && \
			"$param3_qsdk" != *RFS_FEATURE* ]]; then
	    export QSDK_DIR=$param3_qsdk
	else
	    export QSDK_DIR=${SSDK_PATH}/../../../
	fi

	export TOOL_PATH=$(ls -d ${QSDK_DIR}/staging_dir/toolchain*/bin | head -n 1)
	if [ -z "$TOOL_PATH" ]; then
  		echo "Error: toolchain not found in ${QSDK_DIR}"
  		exit 1
	fi
	export IN_COMPILE=$(ls -d ${QSDK_DIR}/staging_dir/toolchain*/bin/*linux-musl*-gcc | xargs -i basename {} | cut -d'-' -f1-4)-
	if [ -z "$IN_COMPILE" ]; then
  		echo "Error: gcc not found in ${QSDK_DIR}"
  		exit 1
	fi

	export IN_INC=$(ls -d ${QSDK_DIR}/staging_dir/target*/usr/include| head -n 1)
	if [ -z "$IN_INC" ]; then
  		echo "Error: depend header file not found in ${QSDK_DIR}"
  		exit 1
	fi

	export IN_ARCH=$( [ $IN_COMPILE = "aarch64-openwrt-linux-musl-" ] && echo "arm64" || echo "arm" )
	export SYS_PATH=$(ls -d ${QSDK_DIR}/qca/src/linux* | head -n 1)
	if [ -z "$SYS_PATH" ]; then
  		echo "Error: Linux not found in ${QSDK_DIR}"
  		exit 1
	fi
	export STAGING_DIR=${QSDK_DIR}/staging_dir
	export PATH=${PATH}:${TOOL_PATH}
}

################################################################################
#               sub functions
################################################################################
check_soc () {
    case $1 in
        'APPE')
            IN_SOC=ipq95xx
            ;;
        'HPPE')
            IN_SOC=ipq807x
            ;;
        'CPPE')
            IN_SOC=ipq60xx
            ;;
        'MPPE')
            IN_SOC=ipq53xx
            ;;
        'MRPPE')
            IN_SOC=ipq54xx
            ;;
         *)
            echo "error: No such CHIP type [$1]"
            exit 1
            ;;
    esac
    export IN_CHIP=$1
    export IN_SOC
}

ssdk_compile() {
    echo "-----------------------------------------------------------------------------"
    echo "       Start to Compile SSDK $IN_SOC $IN_CHIP with $IN_COMPILE          "
    echo "-----------------------------------------------------------------------------"
    make -C ${SSDK_PATH}/mk MODULE_TYPE=KSLIB modules \
    TOOL_PATH=${TOOL_PATH} SYS_PATH=${SYS_PATH} TOOLPREFIX=${IN_COMPILE} KVER=${SYS_PATH##*-} \
    ARCH=${IN_ARCH} TARGET_SUFFIX="musl" SoC=${IN_SOC} CHIP_TYPE=${IN_CHIP} \
    ${param4_option} \
    \
    LNX_MAKEOPTS='-C ${SYS_PATH} KCFLAGS="-fno-caller-saves" HOSTCFLAGS="-O2 \
    -Wall -Wmissing-prototypes -Wstrict-prototypes" CROSS_COMPILE="${IN_COMPILE}" ARCH="${IN_ARCH}" \
    KBUILD_HAVE_NLS=no KBUILD_BUILD_USER="" KBUILD_BUILD_HOST="" KBUILD_BUILD_VERSION="0" \
    CONFIG_SHELL="bash" V=''  cmd_syscalls= KBUILD_EXTRA_SYMBOLS="" MYSOC=${IN_SOC} \
    EXTRA_CFLAGS=-I${IN_INC}' MAKEFLAGS=${makeflags}

	wait
    if [ $? -ne 0 ]; then
	 	exit 1
    fi
    echo "-----------------------------------------------------------------------------"
    echo "       SSDK Compile Done!                                                    "
    echo "-----------------------------------------------------------------------------"
}

ssdk_strip() {
    CONFIG_STRIP_ARGS="--strip-unneeded --remove-section=.comment --remove-section=.note"
    ${IN_COMPILE}strip ${CONFIG_STRIP_ARGS} qca-ssdk.ko

	#wait
    if [ $? -ne 0 ]; then
	 	exit 1
    fi
    echo "-----------------------------------------------------------------------------"
    echo "       SSDK Strip Done!                                                      "
    echo "-----------------------------------------------------------------------------"
}

ssdk_clean() {
    find ../ -regextype posix-extended -regex ".*\.(o|cmd|mod|mod\.c|symvers|order)" -delete
    rm -f qca-ssdk.ko

    echo "-----------------------------------------------------------------------------"
    echo "       SSDK Clean Done!                                                      "
    echo "-----------------------------------------------------------------------------"
}

ssdk_test() {
	#####################################################################################
	#	Profile file example:
	#    APPE /path_to_qsdk_32  "MINI_SSDK=enable LOWMEM_256=enable"
	#    MRPPE /path_to_qsdk_64
	#
	#####################################################################################
	profile_name=$1
	i=1
	rm -rf test_obj
	mkdir test_obj
	cp $profile_name test_obj/
	while read -r line
	do
	    if [[ $line =~ ^\s*$ ]] || [[ $line == \#* ]]; then
	        continue
	    fi
	   echo  "##############################################################################"
	   echo  "#($i) $line#"
	   echo  "##############################################################################"
	   read p1 p2 p3 <<< $(echo $line | awk -v q="'" '{print $1, $2, substr($0, index($0,$3))}')
	   #echo  "#($i) $p1 $p2 $p3##########"
	   ./make.sh clean
	   p3=${p3//\"/}
	   ./make.sh compile $p1 $p2 "$p3"
  	   if [ $? -ne 0 ]; then
  	   		echo "$line  falied!"
  		 	exit 1
  	   fi

	   cp qca-ssdk.ko test_obj/qca-ssdk_$i.ko
	   echo $line > test_obj/qca-ssdk_$i.txt
	   i=$((i + 1))
	done < $profile_name

    echo "-----------------------------------------------------------------------------"
    echo "       SSDK Test Done!                                                      "
    echo "-----------------------------------------------------------------------------"
}

ssdk_usage() {
    echo "-----------------------------------------------------------------------------"
    echo "       Usage                                                                 "
    echo "-----------------------------------------------------------------------------"

    echo "./make.sh <compile> <APPE|HPPE|CPPE|MPPE|MRPPE> [qsdk_location] [-j31] [MINI_SSDK=enable] [LOWMEM_256=enable] [HNAT_FEATURE=enable] [RFS_FEATURE=enable] [-j31]"
    echo "./make.sh <test> <profile_name> [-j31]"
    echo "./make.sh <clean>"
}

ssdk_main (){
	if [[ "$paramlast_flags" = -j* ]]; then
		export makeflags=$paramlast_flags
	fi

    if [ "$1" = "compile" ];then # no support [ "$1" = "" ]
    	prepare_compile
    	check_soc $param2_profile
        ssdk_compile
        ssdk_strip
    elif [ "$1" = "clean" ];then
        ssdk_clean
    elif [ "$1" = "test" ];then
    	ssdk_test $param2_profile
    else
        ssdk_usage
    fi
}

################################################################################
#               main function
################################################################################
ssdk_main $param1_action

