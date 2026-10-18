#!/bin/sh
#
# Copyright (c) 2015, 2020, The Linux Foundation. All rights reserved.
# Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

get_config_file_path()
{
	local file_type brd_name board
	local ini_path
	local caldata_path

	if [[ $# -ne 1 ]]; then
		return
	fi

	file_type="$1"

	case "$file_type" in
	ini|caldata) ;;
	*) return ;;
	esac

	[ -f /tmp/sysinfo/board_name ] && {
		brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
		board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
	}

	case "$board" in
	ap-sdxlemur* | ap-sdxpinn*)
		ini_path="/etc/misc/ipq/ini"
		caldata_path="/data/vendor/wifi/caldata"
	;;
	*)
		ini_path="/ini"
		caldata_path="/lib/firmware"
	;;
	esac

	case "$file_type" in
	ini)
		echo "$ini_path"
	;;
	caldata)
		echo "$caldata_path"
	;;
	esac
}

create_cfg_caldata() {
	local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
	local brd=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
	local fw_caldata=$(get_config_file_path "caldata")

	awk -F ',' -v apdk='/tmp/' -v mtdblock=$1 -v ahb_dir=$2 -v pci_dir=$3 -v pci1_dir=$4 -v board=$brd -v fw_path=$fw_caldata '{
		if ($1 == board) {
			print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6
                        file_suffix=$6+1
			BDF_SIZE=0
			if ($6 == 255) {
				print "Internal radio"
				cmd ="stat -Lc%s " fw_path "/" ahb_dir "/bdwlan.b" $2 " 2> /dev/null"
				cmd | getline BDF_SIZE
				close(cmd)
				if(!BDF_SIZE) {
					print "BDF file for Board id " $2 " not found. Using default value"
					BDF_SIZE=131072
				}
				cmd = "dd if="mtdblock" of=" apdk ahb_dir "/caldata.bin bs=1 count=" BDF_SIZE " skip=" $4
				system(cmd)
				cmd = "cp " apdk ahb_dir "/caldata.bin " fw_path "/" ahb_dir "/"
				system(cmd)
			} else {
				print "PCI radio"
				dir_lib=pci_dir
				if ($3 == 2){
					print "Inside slot instance 2"
					if (pci1_dir != 0) {
						dir_lib=pci1_dir
					}
				}
				cmd ="stat -Lc%s " fw_path "/" dir_lib "/bdwlan.b" $2 " 2> /dev/null"
				cmd | getline BDF_SIZE
				close(cmd)
				if(!BDF_SIZE) {
					print "BDF file for Board id " $2 " not found. Using default value"
					if (dir_lib == "qcn9224")
						BDF_SIZE=184320
					#Adding additional condition check for pebble wideband case
					else if (dir_lib == "qcn6432" && $2 == 0070)
						BDF_SIZE=168960
					else
						BDF_SIZE=131072
				}
				cmd = "dd if="mtdblock" of=" apdk dir_lib "/caldata_" file_suffix ".b" $2 " bs=1 count=" BDF_SIZE " skip=" $4
				system(cmd)
				cmd = "cp " apdk dir_lib "/caldata_" file_suffix ".b" $2 " " fw_path "/" dir_lib "/"
				system(cmd)
			}
		}
	}' $fw_caldata/ftm.conf

	case "$brd" in
	ap-sdxpinn*)
		;;
	*)
		[ -f $fw_caldata/$2/caldata.bin ] || touch $fw_caldata/$2/caldata.bin
		;;
	esac

}

do_ftm_conf_override()
{
        #Necessary conditon check, This method will be invoked only for below mentioned RDP's
        #Inside this API, we will update the ftm.conf file with DTS board ID values maintained.
        #This is applicable only for below mentioned RDP's, For other RDP's return [Do nothing]
        local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
        local board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
        local ftm_conf_path=$(get_config_file_path "caldata")
        local board_id_2g
        local board_id_5g
        local board_id_6g
        local ker_ver=`uname -r |cut -d. -f1`

        if [ $ker_ver -ge 6 ]; then
            case "$board" in
                    ap-mi04.3*|ap-mi04.1*|ap-mi01.3*|ap-mi01.14)
                    board_id_2g=`hexdump -C /proc/device-tree/soc@0/wifi@c0000000/qcom,board_id | awk '{print $5}'`
                    board_id_5g=`hexdump -C /proc/device-tree/soc@0/wifi1@c0000000/qcom,board_id | awk '{print $5}'`
                    board_id_6g=`hexdump -C /proc/device-tree/soc@0/wifi2@c0000000/qcom,board_id | awk '{print $5}'`
                    case "$board" in
                            ap-mi01.14)
                            board_id_6g=`hexdump -C /proc/device-tree/soc@0/wifi3@f00000/board_id | awk '{print $5}'`
                                    ;;
                    esac
                            ;;
                    *)
                            echo "Board name is $board -do_ftm_conf_override API not applicable" > /dev/console && return
                    ;;
            esac
        else
            case "$board" in
                    ap-mi04.3*|ap-mi04.1*|ap-mi01.3*|ap-mi01.14)
                    board_id_2g=`hexdump -C /proc/device-tree/soc/wifi@c0000000/qcom,board_id | awk '{print $5}'`
                    board_id_5g=`hexdump -C /proc/device-tree/soc/wifi4@f00000/qcom,board_id | awk '{print $5}'`
                    board_id_6g=`hexdump -C /proc/device-tree/soc/wifi5@f00000/qcom,board_id | awk '{print $5}'`
                    case "$board" in
                            ap-mi01.14)
                            board_id_5g=`hexdump -C /proc/device-tree/soc/wifi1@f00000/qcom,board_id | awk '{print $5}'`
                            board_id_6g=`hexdump -C /proc/device-tree/soc/wifi2@f00000/board_id | awk '{print $5}'`
                                    ;;
                    esac
                            ;;
                    *)
                            echo "Board name is $board -do_ftm_conf_override API not applicable" > /dev/console && return
                    ;;
            esac
        fi
        awk -F',' -v board=$board -v board_id_2g=$board_id_2g -v board_id_5g=$board_id_5g -v board_id_6g=$board_id_6g -v ftm_conf_path=$ftm_conf_path '{
                if ($1 == board) {
                        print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" NR
                        lineNumber=NR
                        if ($3 == 0){
                                print "2G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_2g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" board_id_2g "\/ " ftm_conf_path "/ftm.conf"
                        }
                        if ($3 == 1){
                                print "5G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_5g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_5g "\/ " ftm_conf_path "/ftm.conf"
                        }
                        else if($3 == 2)
                        {
                                print "6G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_6g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_6g "\/ " ftm_conf_path "/ftm.conf"
                        }
                        system(cmd)
                }
        }' $ftm_conf_path/ftm.conf
}

#create_cfg_caldata_mr is the new api added for multi radio support
#To call this API, ftm.conf entry should have DIR argument with the existing arguments 
#while calling it should have 2 aruguments mtdblock and integrated radio
#Ex : create_cfg_caldata_mr "${mtdblock}" "Integrated radio"

create_cfg_caldata_mr()
{
    local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
    local brd=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
    local ftm_conf_path=$(get_config_file_path "caldata")
    local grep_val=$(grep $brd $ftm_conf_path/ftm.conf)
    local num_rows="$(grep -w -c $brd $ftm_conf_path/ftm.conf)"
    local apdk="/tmp"

    # Loop to process the output
    for i in `seq 1 $num_rows`
    do

        #Parse the FTM.conf file and Get the Values
        ROW_VAL=$(echo $grep_val | awk -v i=$i '{print $i}')
        BOARD_ID=$(echo $ROW_VAL | awk -F ',' '{print $2}')
        SLOT_ID=$(echo $ROW_VAL | awk -F ',' '{print $3}')
        OFFSET=$(echo $ROW_VAL | awk -F ',' '{print $4}')
        SIZE=$(echo $ROW_VAL | awk -F ',' '{print $5}')
        IS_PCI=$(echo $ROW_VAL | awk -F ',' '{print $6}')
        DIR_LIB=$(echo $ROW_VAL | awk -F ',' '{print $7}')

        echo -e $brd "\t" $BOARD_ID "\t"  $SLOT_ID "\t" $OFFSET "\t" $SIZE "\t" $IS_PCI "\t" $DIR_LIB

        #Get the BDF size
        BDF_SIZE=$(stat -Lc%s /lib/firmware/"$DIR_LIB"/bdwlan.b"$BOARD_ID")

        if [ -z $BDF_SIZE ]
        then
            BDF_SIZE=$SIZE
        fi

        echo "BDF_SIZE -" $BDF_SIZE

        if [ $IS_PCI == "255" ]
        then
            cmd=$(dd if=$1 of="$apdk"/"$DIR_LIB"/caldata.bin bs=1 count="$BDF_SIZE" skip="$OFFSET")
            cp -f "$apdk"/"$DIR_LIB"/caldata.bin /lib/firmware/"$DIR_LIB"/
        else
            cmd=$(dd if=$1 of="$apdk"/"$DIR_LIB"/caldata_"$SLOT_ID".b"$BOARD_ID" bs=1 count="$BDF_SIZE" skip="$OFFSET")
            cp -f "$apdk"/"$DIR_LIB"/caldata_"$SLOT_ID".b"$BOARD_ID" /lib/firmware/"$DIR_LIB"/
        fi

        [ -f $ftm_conf_path/$2/caldata.bin ] || touch $ftm_conf_path/$2/caldata.bin
    done
}
