/*
 * INTF - Interference
 * AWGN - Additive white Gaussian Noise
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2013-2017, Qualcomm Atheros, Inc.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided that
 * the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without specific
 *   prior written permission.
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef CONFIG_QCN_EXTN
int hostapd_intf_awgn_detected(struct hostapd_iface *iface, int freq,
			        int chan_width,
			        int cf1, int cf2,
			        u32 chan_bw_interference_bitmap);

int hostapd_intf_afc_received(struct hostapd_iface *iface);
#endif /* CONFIG_QCN_EXTN */

bool hostapd_is_backhaul_sta_conn(struct hostapd_iface *iface);


/*
 * hostapd_afc_chan_sel_cond - enum to set channel selction config
 * values, which will use to validate afc request
 * @HOSTAPD_AFC_CHAN_SEL_CUR_PWR_LT_AFC_PWR - AFC request is valid
 * if current eirp is less than afc eirp.
 * @HOSTAPD_AFC_CHAN_SEL_CUR_PWR_EQ_AFC_PWR - AFC request is valid
 * if current eirp is equal to afc eirp.
 * @HOSTAPD_AFC_CHAN_SEL_CUR_PWR_GT_AFC_PWR - AFC request is valid
 * if current eirp is greater than afc eirp.
 * @HOSTAPD_AFC_CHAN_SEL_ALL - AFC request is valid in all cases.
 */
enum hostapd_afc_chan_sel_cond {
	HOSTAPD_AFC_CHAN_SEL_CUR_PWR_LT_AFC_PWR = BIT(0),
	HOSTAPD_AFC_CHAN_SEL_CUR_PWR_EQ_AFC_PWR = BIT(1),
	HOSTAPD_AFC_CHAN_SEL_CUR_PWR_GT_AFC_PWR = BIT(2),
	HOSTAPD_AFC_CHAN_SEL_ALL = (HOSTAPD_AFC_CHAN_SEL_CUR_PWR_LT_AFC_PWR |
				    HOSTAPD_AFC_CHAN_SEL_CUR_PWR_EQ_AFC_PWR |
				    HOSTAPD_AFC_CHAN_SEL_CUR_PWR_GT_AFC_PWR),
};
