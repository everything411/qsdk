/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_ath_client_warn(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_ath_client_info(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_ath_client_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_ATH_CLIENT_DEBUG_LEVEL < 2)
#define nss_ppe_ath_client_warn(s, ...)
#else
#define nss_ppe_ath_client_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_ATH_CLIENT_DEBUG_LEVEL < 3)
#define nss_ppe_ath_client_info(s, ...)
#else
#define nss_ppe_ath_client_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_ATH_CLIENT_DEBUG_LEVEL < 4)
#define nss_ppe_ath_client_trace(s, ...)
#else
#define nss_ppe_ath_client_trace(s, ...) \
	pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */
