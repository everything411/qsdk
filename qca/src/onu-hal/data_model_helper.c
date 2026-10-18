/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include "data_model_helper.h"
#include <inttypes.h>

uint64_t res;
char retValue[256] = {'\0'};

int execute(char *command, char *result)
{
    FILE *fp = NULL;
    char output[256] = {'\0'};
    char *str = NULL,*pos=NULL;
    fp = popen(command, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
        return -1;
    }
    
    size_t bytesRead = fread(output, 1, sizeof(output) - 1, fp);
    if (bytesRead == 0) {
        pclose(fp);
        return -1;
    } else {
        output[bytesRead] = '\0'; // Null-terminate the buffer
    }
    // fgets(output, sizeof(output)-1, fp);
    if (NULL != (pos=strchr(output, '\n')) ) {
        *pos = '\0';
    }
    pclose(fp);
    snprintf(result, sizeof(output), "%s", output);
    return 0;
}

uint64_t execute_int(const char* cmd)
{
    FILE *fp = NULL;
    char output[256] = {'\0'};
    uint64_t res;
    char *str = NULL,*pos=NULL;
    fp = popen(cmd, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
        return -1;
    }
    size_t bytesRead = fread(output, 1, sizeof(output) - 1, fp);
    if (bytesRead == 0) {
        pclose(fp);
        return -1;
    } else {
        output[bytesRead] = '\0'; 
    }
    sscanf(output, "%" SCNu64, &res);
    pclose(fp);
    return res;
}

int gpon_get_gem_transmittedframes(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "cat /proc/trex/pmhd/gpon | grep 'tx frames'  -A65536 | awk '{print $4}' | awk 'NR==%d' | awk '{sum += $1} END {print sum}'", index+3);
    result = execute_int(cmd);
    return result;
}

int gpon_get_gem_receivedframes(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "cat /proc/trex/pmhd/gpon | grep 'rx frames'  -A65536 | awk '{print $9}' | awk 'NR==%d' | awk '{sum += $1} END {print sum}'", index+3);
    result = execute_int(cmd);
    return result;
}

char* gpon_get_ploam_txmessagecount()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR;
    char *err;
    long sumhecerr=0;
    printf("================ inside txmessagecount : sumhecerr is : %lu ==============\n", sumhecerr);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_bst_prof' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_assign_onuid' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_ranging_time' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_deact_onuid'  | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_dis_sn' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_assign_allocid' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_sleep_allow' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_adjust_tx_wl' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_sys_prof' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_protec_ctl' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_pwr_cons_inq' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_proprietary' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_mic_err' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_req_reg' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_key_ctl' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_cali_req' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_tuning_ctl' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_ch_prof' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_change_pwrl' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_reboot' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_unknown' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
        return "-1";
    }
    else
    {
        memset(retValue, 0, 256);
        snprintf(retValue, sizeof(retValue), "%lu", sumhecerr);
        return retValue;
    }
}


uint64_t gpon_get_ploam_txmesgcount()
{
    uint64_t result=0;
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_bst_prof' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_assign_onuid' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_ranging_time' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_deact_onuid'  | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_dis_sn' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_assign_allocid' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_sleep_allow' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_adjust_tx_wl' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_sys_prof' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_protec_ctl' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_pwr_cons_inq' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_proprietary' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_mic_err' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_req_reg' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_key_ctl' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_cali_req' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_tuning_ctl' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_ch_prof' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_change_pwrl' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_reboot' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamd event' -A 11 | grep 'msg_unknown' | awk '{print$5}'");
    return result;
}

uint64_t gpon_get_ploam_rxmesgcount()
{
    uint64_t result=0;
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_sn_onu' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_reg' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_ack' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_key_rpt' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_pwr_cons_rpt' | awk '{print$2}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_reg_zseq' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_ack_zseq' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_tuning_resp' | awk '{print$5}'");
    result+=execute_int("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_sleep_req' | awk '{print$5}'");
    return result;
}

char* gpon_get_ploam_rxmessagecount()
{
    char sv[32] = {'\0'};
    int ret = RETURN_ERR;
    char *err;
    long sumhecerr=0;
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_sn_onu' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_reg' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_ack' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_key_rpt' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_pwr_cons_rpt' | awk '{print$2}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_reg_zseq' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_ack_zseq' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_tuning_resp' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    ret = execute("cat /proc/trex/gpon/edr | grep -w 'ploamu event' -A 5 | grep 'msg_sleep_req' | awk '{print$5}'", sv);
    sumhecerr+=strtoul(sv,&err,10);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
        return "-1";
    }
    else
    {
        memset(retValue, 0, 256);
        snprintf(retValue, sizeof(retValue), "%lu", sumhecerr);
        return retValue;
    }
}

char* gpon_get_gem_directions(int index)
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "omci -c 'gpon gem' | grep Mcast -A65536 | awk '{print $10}' | awk 'NR==%d'", index+3);
    ret = execute(cmd, sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        if(strcmp(sv,"0")==0)
        {
            memset(retValue, 0, 256);
            strlcpy(retValue, "Bidirectional", strlen("Bidirectional")+1);
            return retValue;
        }  
        else
        {
            memset(retValue, 0, 256);
            strlcpy(retValue, "Unidirectional", strlen("Unidirectional")+1);
            return retValue;
        }
    }
    return "-1";
}

char* gpon_get_gem_traffctype(int index)
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR;
    int res=RETURN_ERR;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "cat /proc/trex/gpon/xgem | grep 'encrypt' | grep 'intid=' | awk '{print$7}' | awk 'NR==%d' | cut -d '(' -f2 | cut -d ')' -f1", index+1);
    ret = execute(cmd, sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        res=strcmp(sv,"ucast");
        if(res==0)
        {
            memset(retValue, 0, 256);
            strlcpy(retValue, "Unidirectional", strlen("Unidirectional")+1);
            return retValue;
        }
        else
        {
            memset(retValue, 0, 256);
            strlcpy(retValue, "Bidirectional", strlen("Bidirectional")+1);
            return retValue;
        }
    }
    return "-1";
}

uint64_t gpon_get_unicast_packets_sent()  //ifIn UcastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifIn/,/ifOut/ {if ($2 ~ /^[0-9]+$/) sum += $3} END {print sum}'");
    return result;
}

uint64_t gpon_get_unicast_packets_received()  //ifOut UcastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifOut/,0 {if ($2 ~ /^[0-9]+$/) sum += $3} END {print sum}'");
    return result;
}

uint64_t gpon_get_discard_packets_sent()  //ifIn Discard
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifIn/,/ifOut/ {if ($2 ~ /^[0-9]+$/) sum += $6} END {print sum}'");
    return result;
}

uint64_t gpon_get_discard_packets_received()  //ifOut Discard
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifOut/,0 {if ($2 ~ /^[0-9]+$/) sum += $6} END {print sum}'");
    return result;
}

uint64_t gpon_get_multicast_packets_sent()  //ifIn McastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifIn/,/ifOut/ {if ($2 ~ /^[0-9]+$/) sum += $4} END {print sum}'");
    return result;
}

uint64_t gpon_get_multicast_packets_received()  //ifOut McastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifOut/,0 {if ($2 ~ /^[0-9]+$/) sum += $4} END {print sum}'");
    return result;
}

uint64_t gpon_get_broadcast_packets_sent() //ifIn BcastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifIn/,/ifOut/ {if ($2 ~ /^[0-9]+$/) sum += $5} END {print sum}'");
    return result;
}

uint64_t gpon_get_broadcast_packets_received()  //ifOut BcastPkts
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt pkt' | awk '/ifOut/,0 {if ($2 ~ /^[0-9]+$/) sum += $5} END {print sum}'");
    return result;
}

uint64_t gpon_get_eth_packets_sent()
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt' | awk '{sum += $3} END {print sum}'");
    return result;
}

uint64_t gpon_get_eth_packets_received()
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt' | awk '{sum += $2} END {print sum}'");
    return result;
}

uint64_t gpon_get_eth_errors_sent()
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt' | awk '{sum += $7} END {print sum}'");
    return result;
}

uint64_t gpon_get_eth_errors_received()
{
    uint64_t result;
    result=execute_int("omci -c 'switch pkt' | awk '{sum += $6} END {print sum}'");
    return result;
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - nominal_bit_rate_upstream
char* gpon_get_nominal_bitrate()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/global | grep 'us_rate' | cut -d'(' -f2 | cut -d')' -f1", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

uint64_t gpon_get_nominalbitrate()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/gpon/global | grep 'us_rate' | cut -d'(' -f2 | cut -d')' -f1 | tr -cd '0-9\n'");
    return result;
}

// xpon_onu.{i} - equipment id
char* gpon_get_equipment_id()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'mib me 257' | grep Equipment_id | awk '{print $3}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i} - name
char* gpon_get_onu_name()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'mib me 256' | grep Serial_number | cut -d' ' -f4 | tr -d ','", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - rx_power
char* gpon_get_rxp_signal_level()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'info anig' | grep rx | awk '{print $2 $3 $4}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

int gpon_get_rxp_signallevel()
{
    int result;
    result=execute_int("omci -c 'info anig' | grep rx | awk '{print $2 $3 $4}' | cut -d '(' -f2 | cut -d')' -f1 | sed 's/dBm//'");
    result=result*10;
    return result;
}

int gpon_get_txp_signallevel()
{
    int result;
    result=execute_int("omci -c 'info anig' | grep tx | awk '{print $2 $3 $4}' | cut -d '(' -f2 | cut -d')' -f1 | sed 's/dBm//'");
    result=result*10;
    return result;
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - tx_power
char* gpon_get_txp_signal_level()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'info anig' | grep tx | awk '{print $2 $3 $4}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - voltage
char* gpon_get_current_voltage()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'info anig' | grep vcc | awk '{print $2 $3}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

int gpon_get_voltage()
{
    int result;
    result=execute_int("omci -c 'info anig' | grep vcc | awk '{print $2 $3}' | tr -cd '0-9\n'");
    return result;
}

int gpon_get_bias()
{
    int result;
    result=execute_int("omci -c 'info anig' | grep bias | awk '{print $2 $3}' | tr -cd '0-9\n'");
    return result;

}

int gpon_get_temperature()
{
    int result;
    result=execute_int("omci -c 'info anig' | grep degree | awk '{print $2 $3}' | tr -d 'C'");
    return result;
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - bias
char* gpon_get_current_bias()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'info anig' | grep bias | awk '{print $2 $3}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.transceiver.{i} - temperature
char* gpon_get_current_temperature()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'info anig' | grep degree | awk '{print $2 $3}'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i} - version
// xpon_onu.{i}.software_image.{i} - version
// xpon_onu.{i}.ani.{i}.transceiver.{i} - module_version
char* gpon_get_pm_modulefirmwareversion()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /etc/openwrt_version", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i} - status
char* gpon_get_PM_status()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR;
    
    ret = execute("cat /proc/trex/gpon/onu | grep -w state | cut -c 7-8", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        if(strcmp(sv,"O1") == 0)
	   return "Down";
        else if(strcmp(sv,"O5") == 0)
	   return "Up";
        else
	   return "NotPresent";

    }
    return "-1";
}

// xpon_onu.{i}.ani.{i} - pon_mode
// xpon_onu.{i}.ani.{i}.transceiver.{i} - pon_mode
char* gpon_get_PM_ponmode()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/global | grep -w gpon_mode | awk '{print $2}' | cut -d '(' -f2- | cut -d ')' -f1", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.tc.onu_activation - onu_state
char*  gpon_get_pm_operationalstate()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/onu | grep -w state | cut -c 7-8", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.tc.onu_activation - vendor_id
char* gpon_get_ploam_vendorid()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/onu | grep 'sn' | cut -d'=' -f2- | cut -c 1-4", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        printf("sreehari: retvalue=%s\n", retValue);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.tc.onu_activation - serial_number
char* gpon_get_ploam_serialnumber()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/onu | grep 'sn' | cut -d'=' -f2-", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

// xpon_onu.{i}.ani.{i}.stats - 
// bytes_sent, bytes_received, unicast_packets_sent, unicast_packets_received, 
// multicast_packets_sent, multicast_packets_received, broadcast_packets_sent, broadcast_packets_received
uint64_t gpon_get_onu_ani_stats(const char* keyword)
{
    FILE *fp = NULL;
    char output[256] = {'\0'};
    uint64_t res;
    char *str = NULL,*pos=NULL;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat /proc/trex/pmhd/l2dsrv | awk '{for(i=1;i<=NF;i++) if($i==\"%s\") print $(i+1)}'", keyword);
    fp = popen(cmd, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
        return -1;
    }
    size_t bytesRead = fread(output, 1, sizeof(output) - 1, fp);
    if (bytesRead == 0) {
        pclose(fp);
        return -1;
    } else {
        output[bytesRead] = '\0'; 
    }
    sscanf(output, "%" SCNu64, &res);
    pclose(fp);
    return res;
}

uint64_t gpon_get_ani_errors_sent()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("us_drop_mem_ov") + gpon_get_onu_ani_stats("us_drop_lkp_ov") + gpon_get_onu_ani_stats("us_drop_treat") + gpon_get_onu_ani_stats("rx_mac_err") + gpon_get_onu_ani_stats("rx_runt") + gpon_get_onu_ani_stats("rx_fcs_err") + gpon_get_onu_ani_stats("rx_pause"));
    return result;
}

uint64_t gpon_get_ani_errors_received()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("ds_drop_mem_ov") + gpon_get_onu_ani_stats("ds_drop_lkp_ov") + gpon_get_onu_ani_stats("ds_drop_treat") + gpon_get_onu_ani_stats("tx_pause"));
    return result;
}

uint64_t gpon_get_ani_stats_packets_sent()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("us_ucast") + gpon_get_onu_ani_stats("us_bcast") + gpon_get_onu_ani_stats("us_mcast")) - gpon_get_onu_ani_stats("us_drop_mem_ov") - gpon_get_onu_ani_stats("us_drop_lkp_ov") - gpon_get_onu_ani_stats("us_drop_treat") - gpon_get_onu_ani_stats("rx_fcs_err") - gpon_get_onu_ani_stats("rx_mac_err") - gpon_get_onu_ani_stats("rx_pause");
    return result;
}

uint64_t gpon_get_ani_stats_packets_received()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("ds_ucast") + gpon_get_onu_ani_stats("ds_bcast") + gpon_get_onu_ani_stats("ds_mcast"));
    return result;
}

uint64_t gpon_get_ani_stats_discard_packets_sent()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("us_drop_mem_ov") + gpon_get_onu_ani_stats("us_drop_lkp_ov") + gpon_get_onu_ani_stats("us_drop_treat"));
    return result;
}

uint64_t gpon_get_ani_stats_discard_packets_received()
{
    uint64_t result;
    result = (gpon_get_onu_ani_stats("ds_drop_mem_ov") + gpon_get_onu_ani_stats("ds_drop_lkp_ov") + gpon_get_onu_ani_stats("ds_drop_treat"));
    return result;
}

// xpon_onu.{i}.ani.{i}.tc.pm.phy - 
// corrected_fec_bytes, corrected_fec_codewords, uncorrectable_fec_codewords
// total_fec_codewords, psbd_hec_error_count, 
uint64_t gpon_get_onu_ani_tc_pm(const char* keyword)
{
    FILE *fp = NULL;
    char output[256] = {'\0'};
    uint64_t res;
    char *str = NULL,*pos=NULL;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat /proc/trex/pmhd/gpon | awk '{for(i=1;i<=NF;i++) if($i==\"%s\") print $(i+1)}'", keyword);
    fp = popen(cmd, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
        return -1;
    }
    size_t bytesRead = fread(output, 1, sizeof(output) - 1, fp);
    if (bytesRead == 0) {
        pclose(fp);
        return -1;
    } else {
        output[bytesRead] = '\0'; 
    }
    sscanf(output, "%" SCNu64, &res);
    pclose(fp);
    return res;
}


// ****************************************
// xpon_onu.{i}.ani.{i}.tc.pm.ploam

uint64_t gpon_get_ploam_micerrors()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/pmhd/gpon | grep -w ploam_mic_err | awk '{print $3}'");
    return result;
}

uint64_t gpon_get_downstream_message_count()
{
    // uint64_t result;
    // result=execute_int("");
    return -1;
}

uint64_t gpon_get_ranging_time()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/pmhd/gpon | grep ranging_time_msg_cnt | awk '{print $3}'");
    return result;
}

uint64_t gpon_get_upstream_message_count()
{
    // uint64_t result;
    // result=execute_int("");
    return -1;
}
// ****************************************


// ****************************************
// xpon_onu.{i}.ani.{i}.tc.pm.omci

uint64_t gpon_get_omci_baselinemessagecount()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/sys/omci | grep -w 'omci rx' -A 6 | grep -w 'dev_type_a' | awk '{print$2}'");
    return result;
}

uint64_t gpon_get_omci_extendedmessagecount()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/sys/omci | grep -w 'omci rx' -A 6 | grep -w 'dev_type_b' | awk '{print$2}'");
    return result;
}

uint64_t gpon_get_omci_micerrors()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/sys/omci | grep -w 'mic_error' | awk '{print$2}'");
    return result;
}

// ****************************************


// ****************************************
// xpon_onu.{i}.ani.{i}.tc.gem.port.{i}

uint64_t gpon_get_ploam_onuid()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/gpon/onu | grep -w id  | cut -d':' -f2- | cut -d '(' -f1");
    return result;
}

// uint64_t gpon_get_gem_directions()
// {
//     return -1;
// }

// *********************************


// ****************************************
// xpon_onu.{i}.ani.{i}.tc.alarms

// int gpon_get_pm_alarm_tf_old()
// {
//     float num;
//     char sv[256] = {'\0'};
//     int ret = RETURN_ERR, itemp;
//     ret = execute("omci -c 'info anig' | grep tx | awk '{print $2}'", sv);
//     if(RETURN_OK != ret)
//     {
//         printf("\nError %s\n", __func__);
//     }
//     else
//     {
//         if (sscanf(sv, "%f", &num) == 1)
//         {
//             if (num>1995.3 && num<6309.6)
//             {
//                 return 0;
//             }
//             else
//             {
//                 return 1;
//             }
//         }
//     }
//     return -1;
// }

int gpon_get_pm_alarm_tf()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep -w TF | awk '{ print $3 }'");
    return result;
}

int gpon_get_pm_alarm_suf()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep -w SUF | awk '{ print $3 }'");
    return result;
}

int gpon_get_pm_alarm_mem()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep -w MEM | awk '{ print $3 }'");
    return result;
}

int gpon_get_pm_alarm_dact()
{
    char sv[256] = {'\0'}, conustate[256] = {'\0'};
    int ret = RETURN_ERR,result=RETURN_ERR;
    int msgonuid = RETURN_ERR;
    ret = execute("cat /proc/trex/gpon/onu | grep -w state | cut -c 7-8", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        sscanf(sv, "%s", conustate);
        memset(sv, 0, 256);
        ret = execute("cat /proc/trex/gpon/edr | grep msg_deact_onuid -w | awk '{ print$5}'", sv);
        if(RETURN_OK != ret)
        {
            printf("\nError %s\n", __func__);
        }
        else
        {
            sscanf(sv, "%d", &msgonuid);
            // (strcmp(conustate, "O5") != 0)
            if(msgonuid>0 && strcmp(conustate, "O5") != 0)
            {
                // Active
                return 1;
            }
            else
            {
                // Inactive
                return 0;
            }
        }
    }
    return -1;
}

int gpon_get_pm_alarm_los()
{
    uint64_t result;
    result=execute_int("grep -w rx_los /proc/trex/io/pon | awk '{print $2}'");
    if (result == 1)
    {
        return 1;
    }
    return 0;
}

int gpon_get_pm_alarm_lof()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("cat /proc/trex/gpon/onu | grep -w state | cut -c 7-8", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    if (sv == "O1")
    {
        return 1;
    }
    return 0;
}

// uint64_t gpon_get_pm_alarm_lcdg_old()
// {
//     uint64_t result;
//     result=execute_int("cat /proc/trex/pmhd/gpon | grep -w xgem_hd_hec_err  | awk '{print$7}'");
//     return result;
// }

uint64_t gpon_get_pm_alarm_lcdg()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep -w LCDG | awk '{ print $3 }'");
    return result;
}

uint64_t gpon_get_pm_alarm_rogue()
{
    uint64_t result;
    result=execute_int("grep -w tx_fault /proc/trex/io/pon | awk '{print $2}'");
    if (result == 1)
    {
        return 1;
    }
    return 0;
}

int gpon_get_pm_alarm_sf()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep ' SF' | awk {'print $3'}");
    return result;
}

int gpon_get_pm_alarm_sd()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep ' SD' | awk {'print $3'}");
    return result;
}

int gpon_get_gem_port_id(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "omci -c 'gpon gem' | grep Gem -A65536 | awk '{print $1}' | awk 'NR==%d' | cut -d '(' -f1", index+3);
    result=execute_int(cmd);
    return result;
}

int gpon_get_pm_alarm_dis()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR,res=RETURN_ERR;
    int msgonuid = RETURN_ERR;
    ret = execute("cat /proc/trex/gpon/alarm | grep -w DIS | awk '{ print $3 }'", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        sscanf(sv, "%d", &msgonuid);
        memset(sv, 0, 256);
        ret = execute("cat /proc/trex/gpon/alarm | grep -w DIS | awk '{ print $4 }'", sv);
        if(RETURN_OK != ret)
        {
            printf("\nError %s\n", __func__);
        }
        else
        {
            if(msgonuid>0 && strcmp(sv, "-")!=0)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
    return -1;
}

int gpon_get_pm_alarm_lods()
{
    int result;
    result=execute_int("cat /proc/trex/gpon/alarm | grep -w LODS | awk '{ print $3 }'");
    return result;
}

int gpon_get_tc_perf_signal_fail()
{
    int result;
    result=execute_int("omci -c 'mib me 263' | grep -w SF_threshold | awk '{print$3}' | cut -d '(' -f2 |cut -d ')' -f1");
    return result;
}

int gpon_get_tc_perf_signal_degrade()
{
    int result;
    result=execute_int("omci -c 'mib me 263' | grep -w SD_threshold | awk '{print$3}' | cut -d '(' -f2 |cut -d ')' -f1");
    return result;
}

int gpon_get_frame_header_hec_errors()
{
    int result;
    result=execute_int("omci -c 'gpon cnt global' | grep xgem_hd_hec_err= | awk '{print $2}' | cut -d '=' -f2");
    return result;
}

int gpon_get_xgem_key_errors()
{
    int result;
    result=execute_int("omci -c 'gpon cnt global' | grep xgem_key_err= | awk '{print $2}' | cut -d '=' -f2");
    return result;
}

int gpon_get_software_number_of_entries()
{
    int result;
    result=execute_int("omci -c 'mib me 7' | grep -c meid");
    return result;
}

int gpon_get_eth_uni_number_of_entries()
{
    int result;
    result=execute_int("omci -c 'info conv_uni' | grep IF_NAME -A10 | grep -c eth");
    return result;
}

int gpon_get_port_number_of_entries()
{
    int result;
    result=execute_int("omci -c 'gpon gem' | grep -c 0x");
    return result;
}

uint64_t gpon_get_eth_bytes_sent()
{
    uint64_t result1=-1, result2=-1;
    result1=execute_int("fw_printenv | grep machid | cut -d'=' -f2");

    if (result1==8050001 || result1==8060001 || result1==8060002 || result1==8060201)
    {
        result2=execute_int("ethtool -S eth0 | grep tx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    else if(result1==8050003)
    {
        result2=execute_int("ethtool -S eth5 | grep tx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    else if(result1==8050004)
    {
        result2=execute_int("ethtool -S eth4 | grep tx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    return result2;
}

uint64_t gpon_get_eth_bytes_received()
{
    uint64_t result1=-1, result2=-1;
    result1=execute_int("fw_printenv | grep machid | cut -d'=' -f2");

    if (result1==8050001 || result1==8060001 || result1==8060002 || result1==8060201)
    {
        result2=execute_int("ethtool -S eth0 | grep rx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    else if(result1==8050003)
    {
        result2=execute_int("ethtool -S eth5 | grep rx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    else if(result1==8050004)
    {
        result2=execute_int("ethtool -S eth4 | grep rx_bytes: | sed -n '2p' | cut -d':' -f2 | tr -d ' '");
    }
    return result2;
}

int gpon_get_sw_img_is_committed(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "omci -c 'mib me 7' | grep Is_committed | sed -n '%dp' | cut -d'(' -f2 | cut -d')' -f1", index);
    result=execute_int(cmd);
    return result;
}

int gpon_get_sw_img_is_active(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "omci -c 'mib me 7' | grep Is_active | sed -n '%dp' | cut -d'(' -f2 | cut -d')' -f1", index);
    result=execute_int(cmd);
    return result;
}

int gpon_get_sw_img_is_valid(int index)
{
    int result;
    char cmd[256];
    snprintf (cmd, sizeof(cmd), "omci -c 'mib me 7' | grep Is_valid | sed -n '%dp' | cut -d'(' -f2 | cut -d')' -f1", index);
    result=execute_int(cmd);
    return result;
}

char* gpon_get_eth_uni_status()
{
    int result;
    result=execute_int("omci -c 'mib me 11' | grep Pptp_Ethernet_UNI -A10 | grep Operational_state | cut -d'(' -f2 | tr -d ')'");
    
    if(result==0)
    {
        return "Up";
    }
    else if(result==1)
    {
        return "Down";
    }
    return "-1";
}

char* gpon_get_eth_uni_name()
{
    char sv[256] = {'\0'};
    int ret = RETURN_ERR, itemp;
    ret = execute("omci -c 'mib me 11' | grep Pptp_Ethernet_UNI -A2 | grep inst | cut -d',' -f2", sv);
    if(RETURN_OK != ret)
    {
        printf("\nError %s\n", __func__);
    }
    else
    {
        memset(retValue, 0, 256);
        strlcpy(retValue, sv, strlen(sv) + 1);
        return retValue;
    }
    return "-1";
}

char* gpon_get_eth_uni_interdomain_id()
{
    int result;
    result=execute_int("omci -c 'mib me 329' | grep meid | cut -d'(' -f2 | cut -d')' -f1");
    char sv[256] = {'\0'};
    snprintf(sv, sizeof(sv), "(VEIP,%d)", result);
    memset(retValue, 0, 256);
    strlcpy(retValue, sv, strlen(sv) + 1);
    return retValue;
}

uint64_t gpon_get_onu_ani_last_change()
{
    uint64_t result;
    result=execute_int("cat /proc/trex/gpon/pm | grep 'accum time' -A10 | awk '{print $6}' | awk '{sum += $1} END {print sum}'");
    return result;
}

void gpon_set_xpon_onu_enable(int val)
{
    printf("----------- Inside gpon_set_xpon_onu_enable function ------------\n");
    char* cmd;
    if(val==0)
    {
        cmd="/etc/init.d/omci_done stop";
    }
    else
    {
        cmd="/etc/init.d/omci_done start";
    }
    FILE *fp = NULL;
    char output[256] = {'\0'};
    char *str = NULL,*pos=NULL;
    fp = popen(cmd, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
    }
    pclose(fp);
}

void gpon_set_xpon_onu_ani_enable(int val)
{
    char* cmd;
    if(val==0)
    {
        cmd="echo tx_disable output 1 > /proc/trex/io/pon ; echo disable_psync 1 > /proc/trex/gpon/mac";
    }
    else{
        cmd="echo tx_disable output 0 > /proc/trex/io/pon ; echo disable_psync 1 > /proc/trex/gpon/mac";
    }
    FILE *fp = NULL;
    char output[256] = {'\0'};
    char *str = NULL,*pos=NULL;
    fp = popen(cmd, "r");
    if(NULL == fp)
    {
        printf("Failed to run command\n" );
    }
    pclose(fp);
}


int gpon_get_xpon_eth_uni_enable()
{
    int result;
    result=execute_int("omci -c 'mib me 11' | grep Pptp_Ethernet_UNI -A10 | grep Administrative_state | cut -d'(' -f2 | tr -d ')'");
    if(result==0){
        return 1;
    }
    else if(result==1)
    {
        return 0;
    }
    return result;
}