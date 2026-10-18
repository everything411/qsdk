/******************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 ******************************************************************/

#include <stdio.h>
#include "util_ubus.h"

int
main(int argc, char *argv[])
{
	char value[129];
	unsigned int session_expire;
	int ret;

	// create config ucitest for testing
	system("cp /etc/config/system /etc/config/ucitest");

// CLIENT INIT
	if (util_ubus_client_init() != 0)
		return -1;

// SESSSION_LOGIN
	// login with session idle timeout 60s
	ret = util_ubus_session_login("root", "admin", 600);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// SESSION_VALIDATE, ret != 0 if session is invalid, session_expire means seconds to expire the rpcd session
	ret = util_ubus_session_validate(&session_expire);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
#if 0
// SESSSION DESTROY
	// util_ubus_destroy is a must if util_ubus_session_login timeout is 0
	ret = util_ubus_session_destroy();
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
#endif

#if 0
// UCI GET
	// uci get single value from section server
	ret = util_ubus_uci_get("ucitest", "ntp", "server", value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

	// uci get single value from exist section and with option NULL
	ret = util_ubus_uci_get("ucitest", "@system[0]", NULL, value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

	// uci get single value from none exist section and with option NULL
	ret = util_ubus_uci_get("ucitest", "@system[2]", NULL, value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

	// uci get single value from unamed section
	ret = util_ubus_uci_get("ucitest", "@system[0]", "hostname", value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

	ret = util_ubus_uci_get("ucitest", "@system[0]", "cronloglevel", value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

// UCI GET NON-EXIST ENTRY
	// uci get single value from section server
	ret = util_ubus_uci_get("ucitest", "ntp2", "server", value, 129);
	dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);

// UCI_SET && REVERT
	// uci set single value
	ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", "ucitest0");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci revert
	ret = util_ubus_uci_revert("ucitest");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

// UCI_SET && APPLY (rollback disabled)
	// uci set single value
	ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", "ucitest1");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	ret = util_ubus_uci_set("ucitest", "@system[0]", "cronloglevel", "1");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci set list values
	{
		char *values[] = { "1.ucitest.org", "11.ucitest.org", "111.ucitest.org" };
		ret = util_ubus_uci_set_list("ucitest", "ntp", "server", values, 3);
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	}
	// dump changes
	util_ubus_uci_dump_changes("ucitest");
	// uci apply
	ret = util_ubus_uci_apply(0, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// value change is preserved since ROLLBACK is not called after APPLY

// UCI_SET && APPLY (rollback enabled), then ROLLBACK
	// uci set single value
	ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", "ucitest2");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci apply
	ret = util_ubus_uci_apply(1, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci_rollback
	ret = util_ubus_uci_rollback();
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// value change is lost since ROLLBACK is called after APPLY

// UCI_SET && APPLY (rollback enabled), then AUTO ROLLBACK
	// uci set single value
	ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", "ucitest3");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci apply with auto rollback
	ret = util_ubus_uci_apply(1, 3);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// value change is lost since CONFIRM is not called after APPLY
	sleep(5);		// wait auto rollback to take effect

// UCI_SET && APPLY (rollback_enabled), then CONFIRM
	// uci set single value
	ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", "ucitest4");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// uci apply with auto rollback + confirm
	ret = util_ubus_uci_apply(1, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	ret = util_ubus_uci_confirm();
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	// value change is preserved since CONFIRM is called after APPLY
#endif

// UCI_ADD_SECTION && APPLY (rollback_disabled)
	ret = util_ubus_uci_add_section("ucitest", "sec_type1", "name1001");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	ret = util_ubus_uci_add_section("ucitest", "sec_type1", "name1002");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	ret = util_ubus_uci_add_section("ucitest", "sec_type2", "name2001");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	ret = util_ubus_uci_add_section("ucitest", "sec_type2", "name2002");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// APPLY
	ret = util_ubus_uci_apply(0, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

// UCI_DELETE (1 section)
	// uci delete with section type/name specified
	ret = util_ubus_uci_delete("ucitest", "sec_type1", "name1001", NULL);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// UCI DELETE (multiple section)
	// uci delete with only section type specified, which would remove all sec of the same type
	ret = util_ubus_uci_delete("ucitest", "sec_type2", NULL, NULL);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// APPLY
	ret = util_ubus_uci_apply(0, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

// UCI_DELETE (option)
	// uci delete with section/option name specified
	ret = util_ubus_uci_delete("ucitest", NULL, "@system[0]", "conloglevel");
	ret = util_ubus_uci_delete("ucitest", NULL, "@system[0]", "cronloglevel");
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// APPLY
	ret = util_ubus_uci_apply(0, 30);
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

// UCI SET SPEEDTEST
	// uci set on same option repeatly
	// ps: if the value is different, all uci set would be stored in /var/run/rpcd/uci-<session>/<config> 
	//     which would lead slow response of function when the repeatness over 1000
	// pps: repeatness 1000 : 1sec
	//      repearness 4000 : 8sec
	if (0) {
		char hostname_buf[128];
		int i = 0;

		printf("time0=%lu\n", (unsigned long)time(0));
		for (i = 0; i < 10000; i++) {
			if ((i % 1000) == 999) {
				if (argv[1])
					printf("%s time %d=%lu\n", argv[1], i, (unsigned long)time(0));
				else
					printf("time %d=%lu\n", i, (unsigned long)time(0));
			}
			sprintf(hostname_buf, "ucitest%u", i);
			ret = util_ubus_uci_set("ucitest", "@system[0]", "hostname", hostname_buf);
			if (ret)
				dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
		printf("time1=%lu\n", (unsigned long)time(0));

		ret = util_ubus_uci_get("ucitest", "@system[0]", "hostname", value, 129);
		dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);
	}
// FIND SECTION RELATED
	//test find_parent_by_section_type
	ret = find_parent_by_section_type(NULL, "twireless", "wifi-iface", 1, "twireless", "wifi-device");
	if (ret >= 0) {
		char tmpbuf[129];
		dbprintf("index=%d\n\n", ret);
		snprintf(tmpbuf, 129, "@%s[%d]", "wifi-device", ret);
		ret = util_ubus_uci_get("twireless", tmpbuf, NULL, value, 129);
		dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);
	} else
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

	ret = find_parent_by_section_type(NULL, "twireless", "wifi-iface", 3, "twireless", "wifi-device");
	if (ret >= 0) {
		char tmpbuf[129];
		dbprintf("index=%d\n\n", ret);
		snprintf(tmpbuf, 129, "@%s[%d]", "wifi-device", ret);
		ret = util_ubus_uci_get("twireless", tmpbuf, NULL, value, 129);
		dbprintf("ret=%d(%s), value=%s\n\n", ret, util_ubus_error_str(ret), value);
	} else
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

	// error case
	ret = find_parent_by_section_type(NULL, "twireless", "wifi-iface", 0, "twireless", "wifi-ice");
	if (ret >= 0)
		dbprintf("index=%d\n\n", ret);
	else
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

	// test find_child_by_section_type
	{
		int found_child_section_index[10], i;
		ret =
		    find_child_by_section_type(NULL, "twireless", "wifi-iface", found_child_section_index, 10, "twireless",
					       "wifi-device", 0);
		dbprintf("ret=%d, dump index[]=", ret);
		if (ret >= 0) {
			for (i = 0; i < ret; i++)
				fprintf(stderr, "%d ", found_child_section_index[i]);
			fprintf(stderr, "\n");
		} else {
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
	}

	{
		int found_child_section_index[10], i;
		ret =
		    find_child_by_section_type(NULL, "twireless", "wifi-iface", found_child_section_index, 10, "twireless",
					       "wifi-device", 1);
		dbprintf("ret=%d, dump index[]=", ret);
		if (ret >= 0) {
			for (i = 0; i < ret; i++)
				fprintf(stderr, "%d ", found_child_section_index[i]);
			fprintf(stderr, "\n");
		} else {
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
	}

	// error case 1
	{
		int found_child_section_index[1], i;
		ret =
		    find_child_by_section_type(NULL, "twireless", "wifi-iface", found_child_section_index, 1, "twireless",
					       "wifi-device", 0);
		dbprintf("ret=%d, dump index[]=", ret);
		if (ret >= 0) {
			for (i = 0; i < ret; i++)
				fprintf(stderr, "%d ", found_child_section_index[i]);
			fprintf(stderr, "\n");
		} else {
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
	}

	// error case 2
	{
		int found_child_section_index[10], i;
		ret =
		    find_child_by_section_type(NULL, "twireless", "wifi--iface", found_child_section_index, 10, "twireless",
					       "wifi-device", 0);
		dbprintf("ret=%d, dump index[]=", ret);
		if (ret >= 0) {
			for (i = 0; i < ret; i++)
				fprintf(stderr, "%d ", found_child_section_index[i]);
			fprintf(stderr, "\n");
		} else {
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
	}

	// error case 3
	{
		int found_child_section_index[10], i;
		ret =
		    find_child_by_section_type(NULL, "twireless", "wifi-iface", found_child_section_index, 10, "twireless",
					       "wifi--device", 0);
		dbprintf("ret=%d, dump index[]=", ret);
		if (ret >= 0) {
			for (i = 0; i < ret; i++)
				fprintf(stderr, "%d ", found_child_section_index[i]);
			fprintf(stderr, "\n");
		} else {
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		}
	}
#if 0
// SESSSION DESTROY
	// util_ubus_destroy is a must if util_ubus_session_login timeout is 0
	ret = util_ubus_session_destroy();
	dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
#endif
// CLIENT FINISH
	util_ubus_client_finish();

	return 0;
}
