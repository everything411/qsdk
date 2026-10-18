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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "util_ubus.h"

#define UCI_GET_BUF	4096
#define UCI_LINE_DEF
#define dbprintf_wanif(fmt, args...)      do { if(dbg_level) fprintf(stderr,"%s:%d: %s(): " fmt, util_purename(__FILE__), __LINE__, __FUNCTION__, ##args); } while (0)

int dbg_level = 0;
enum uci_option_type {
	UCI_TYPE_STRING = 0,
	UCI_TYPE_LIST = 1,
};

typedef struct _uci_line_ {
	int cmd_idx;
	char *cmd_str;
	char *package;
	char *section;
	char *option;
	enum uci_option_type type;
	char *value;
} UCI_LINE;

enum uci_command {
	UCI_CMD_ADD,
	UCI_CMD_SET,
	UCI_CMD_DEL,
	UCI_CMD_GET,
	UCI_CMD_SETLIST,
	UCI_CMD_ADDLIST,
	UCI_CMD_DELLIST,
	UCI_CMD_CHANGES,
	UCI_CMD_COMMIT,
	UCI_CMD_RESTART,
	UCI_CMD_RENEW,
	UCI_CMD_RELOAD,
	__UCI_CMD_MAX,
	__UCI_CMD_LAST = __UCI_CMD_MAX - 1
};

char *configName = NULL;

int uci_cmd2index(char *cmd);

void
uci_line_init(UCI_LINE * line, int freept)
{
	if (freept) {
		if (line->cmd_str)
			free(line->cmd_str);
		if (line->package)
			free(line->package);
		if (line->section)
			free(line->section);
		if (line->option)
			free(line->option);
		if (line->value)
			free(line->value);
	}
	memset(line, 0, sizeof(UCI_LINE));
	line->cmd_idx = -1;
}

void
uci_line_dump(UCI_LINE * line)
{
	printf("cmd=%s,package=%s,section=%s,option=%s,value=%s\n",
	       line->cmd_str, line->package, line->section, line->option, line->value);
}

#define MAX(a,b)    (a>b)?a:b

void
check_config_file(char *package)
{
	FILE *fp;
	char filepath[128] = "/etc/config/";
	int needcreate = 0;

	if (configName == NULL) {
		needcreate = 1;
	} else {
		size_t len = MAX(strlen(configName), strlen(package));
		if (strncmp(configName, package, len) != 0) {
			needcreate = 1;
		}
	}
	if (needcreate) {
		if (configName)
			free(configName);
		strcat(filepath, package);
		configName = strdup(package);
		if (access(filepath, F_OK) != 0) {
			if (!(fp = fopen(filepath, "w+")))
				printf("%s create fail!\n", filepath);
			else
				fclose(fp);
		}
	}
}

int
uci_line_parser(char *input_line, UCI_LINE * line)
{
	char *pt;
	strtok(input_line, "\n");

	/* value */
	pt = strchr(input_line, '=');
	if (pt) {
		*pt = 0;
		pt++;
		line->value = strdup(pt);
	}
	pt = strtok(input_line, " ");
	if (pt) {
		line->cmd_idx = uci_cmd2index(pt);
		line->cmd_str = strdup(pt);
	}
	if (line->cmd_idx < 0) {
		printf("cmd not found!\n");
		uci_line_init(line, 1);
		return -1;
	}
	switch (line->cmd_idx) {
	case UCI_CMD_ADD:
		pt = strtok(NULL, " ");
		break;
	case UCI_CMD_CHANGES:
	case UCI_CMD_COMMIT:
	case UCI_CMD_RESTART:
	case UCI_CMD_RENEW:
	case UCI_CMD_RELOAD:
		pt = strtok(NULL, "\n");
		break;
	case UCI_CMD_SET:
	case UCI_CMD_DEL:
	case UCI_CMD_GET:
	case UCI_CMD_SETLIST:
	case UCI_CMD_ADDLIST:
	case UCI_CMD_DELLIST:
		pt = strtok(NULL, ".");
		break;
	default:
		return 0;
		break;
	}
	if (pt) {
		line->package = strdup(pt);
	}
	pt = strtok(NULL, ".");
	if (pt) {
		line->section = strdup(pt);
		check_config_file(line->package);
	}
	pt = strtok(NULL, ".");
	if (pt) {
		line->option = strdup(pt);
	}
	return 0;
}

int
uci_cmd_add(UCI_LINE * line)
{
	int ret;

	if (strncmp(line->package, "network", strlen(line->package)) == 0)
		ret = util_ubus_uci_add_section(line->package, "interface", line->section);
	else
		ret = util_ubus_uci_add_section(line->package, line->section, NULL);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

#define MAX_LIST_NUM 32
int
uci_cmd_setlist(UCI_LINE * line)
{
	int ret, cnt = 0;
	char *strpt = NULL;
	char *arr_value[MAX_LIST_NUM];

	strpt = strtok(line->value, " ");
	while (strpt != NULL) {
		arr_value[cnt] = strpt;
		cnt++;
		if (cnt >= MAX_LIST_NUM)
			break;
		strpt = strtok(NULL, " ");
	}
	if (line->option)
		ret = util_ubus_uci_set_list(line->package, line->section, line->option, arr_value, cnt);
	else
		ret = UBUS_STATUS_NOT_FOUND;
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_set(UCI_LINE * line)
{
	int ret;

	if (line->option)
		ret = util_ubus_uci_set(line->package, line->section, line->option, line->value);
	else
		ret = util_ubus_uci_add_section(line->package, line->value, line->section);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_del(UCI_LINE * line)
{
	int ret;

	ret = util_ubus_uci_delete(line->package, NULL, line->section, line->option);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_add_list(UCI_LINE * line)
{
	int ret, cnt = 0;
	char buf[UCI_GET_BUF];
	char *arr_value[MAX_LIST_NUM], *strpt;

	ret = util_ubus_uci_get(line->package, line->section, line->option, buf, sizeof(buf) - 1);
	dbprintf_wanif("ret=%d(%s)\nbuf=%s\n", ret, util_ubus_error_str(ret), buf);

	strpt = strtok(buf, " ");
	while (strpt != NULL) {
		if (strcmp(strpt, line->value) == 0)
			return 0;
		else
			arr_value[cnt++] = strpt;
		if (cnt >= MAX_LIST_NUM)
			break;
		strpt = strtok(NULL, " ");
	}
	if (cnt < MAX_LIST_NUM)
		arr_value[cnt++] = line->value;
	else
		return -1;
	if (line->option)
		ret = util_ubus_uci_set_list(line->package, line->section, line->option, arr_value, cnt);
	else
		ret = UBUS_STATUS_NOT_FOUND;
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_del_list(UCI_LINE * line)
{
	int ret, i, match = 0, cnt = 0;
	char buf[UCI_GET_BUF];
	char *arr_value[MAX_LIST_NUM], *strpt;

	ret = util_ubus_uci_get(line->package, line->section, line->option, buf, sizeof(buf) - 1);
	dbprintf_wanif("ret=%d(%s)\nbuf=%s\n", ret, util_ubus_error_str(ret), buf);

	strpt = strtok(buf, " ");
	while (strpt != NULL) {
		arr_value[cnt++] = strpt;
		if (cnt >= MAX_LIST_NUM)
			break;
		strpt = strtok(NULL, " ");
	}
	for (i = 0; i < cnt; i++) {
		if (match && i < cnt - 1)
			arr_value[i] = arr_value[i + 1];
		else if (strcmp(arr_value[i], line->value) == 0) {
			match = 1;
			arr_value[i] = arr_value[i + 1];
		}
	}
	if (line->option)
		ret = util_ubus_uci_set_list(line->package, line->section, line->option, arr_value, cnt - match);
	else
		ret = UBUS_STATUS_NOT_FOUND;
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_get(UCI_LINE * line)
{
	int ret;
	char buf[UCI_GET_BUF];
	char *name, *value;

	ret = util_ubus_uci_get(line->package, line->section, line->option, buf, sizeof(buf) - 1);
	dbprintf_wanif("ret=%d(%s)\nbuf=%s\n", ret, util_ubus_error_str(ret), buf);
	if (ret >= 0) {
		if (line->option)
			printf("%s.%s.%s=%s\n", line->package, line->section, line->option, buf);
		else {
			name = strtok(buf, ":");
			if (!name)
				return ret;
			do {
				value = strtok(NULL, ",");
				if (name[0] == '.')
					printf("%s.%s=%s\n", line->package, line->section, value);
				else
					printf("%s.%s.%s=%s\n", line->package, line->section, name, value);
			} while ((name = strtok(NULL, ":")));
		}
	}
	return ret;
}

int
uci_cmd_changes(UCI_LINE * line)
{
	int ret;

	ret = util_ubus_uci_dump_changes(line->package);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_commit(UCI_LINE * line)
{
	int ret;

	if (line->package)
		ret = util_ubus_uci_commit(line->package);
	else
		ret = util_ubus_uci_apply(0, 30);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
	return ret;
}

int
uci_cmd_restart(UCI_LINE * line)
{
	int ret;
	char cmd[64] = { 0 };

	if (line->package) {
		snprintf(cmd, 63, "ifdown %s", line->package);
		ret = system(cmd);
		sleep(1);
		snprintf(cmd, 63, "ifup %s", line->package);
		ret = system(cmd);
	} else {
		ret = system("/etc/init.d/network restart");
	}
	return ret;
}

int
uci_cmd_renew(UCI_LINE * line)
{
	int ret=0;
	char cmd[64] = { 0 };

	if (line->package) {
		snprintf(cmd, 63, "ubus call network.interface.%s renew", line->package);
		ret = system(cmd);
	}
	return ret;
}

int
uci_cmd_reload(UCI_LINE * line)
{
	int ret;

	ret = system("/etc/init.d/network reload");
	return ret;
}

typedef int uci_fn_t(UCI_LINE * line);
typedef struct {
	char *cmdName;
	uci_fn_t *func;
} uci_dispatchTable;

uci_dispatchTable uci_cmd_dispatch[] = {
	[UCI_CMD_ADD] = {"add", uci_cmd_add},
	[UCI_CMD_SET] = {"set", uci_cmd_set},
	[UCI_CMD_DEL] = {"del", uci_cmd_del},
	[UCI_CMD_GET] = {"get", uci_cmd_get},
	[UCI_CMD_SETLIST] = {"set_list", uci_cmd_setlist},
	[UCI_CMD_ADDLIST] = {"add_list", uci_cmd_add_list},
	[UCI_CMD_DELLIST] = {"del_list", uci_cmd_del_list},
	[UCI_CMD_CHANGES] = {"changes", uci_cmd_changes},
	[UCI_CMD_COMMIT] = {"commit", uci_cmd_commit},
	[UCI_CMD_RESTART] = {"restart", uci_cmd_restart},
	[UCI_CMD_RENEW] = {"renew", uci_cmd_renew},
	[UCI_CMD_RELOAD] = {"reload", uci_cmd_reload}
};

int numdispatch = sizeof(uci_cmd_dispatch) / sizeof(uci_cmd_dispatch[0]);

int
uci_cmd2index(char *cmd)
{
	for (int i = UCI_CMD_ADD; i < __UCI_CMD_MAX; i++) {
		if (!strncmp(cmd, uci_cmd_dispatch[i].cmdName, strlen(cmd)))
			return i;
	}
	return -1;
}

int
uci_run(UCI_LINE * line)
{
	int ret = -1;

	if (line->cmd_idx >= UCI_CMD_ADD && line->cmd_idx < __UCI_CMD_MAX)
		ret = uci_cmd_dispatch[line->cmd_idx].func(line);
	else
		printf("uci command not found!\n");
	return ret;
}

void
print_help(void)
{
	printf("util_uci: a uci like tool that uses rpcd session for config access\n\n"
	       "syntax:\n"
	       "util_uci [rdonly cmds...]\n"
	       "util_uci [-f <filename>]\n"
	       "util_uci << EOF\n"
	       "...\n"
	       "EOF\n"
	       "\n"
	       "supported cmd:\n"
	       "add      <config> <section>\n"
	       "set      <config>.<section>[.<option>]=<value>\n"
	       "get      <config>.<section>[.<option>]\n"
	       "del      <config>.<section>[.<option>]\n"
	       "set_list <config>.<section>.<option>=<value1> <value2> <value3>\n"
	       "add_list <config>.<section>.<option>=<value>\n"
	       "del_list <config>.<section>.<option>=<value>\n"
	       "changes  <config>\n"
	       "commit  [<config>]\n"
	       "restart [lan | wan]\n"
	       "renew 	[lan | wan]\n"
	       "reload\n"
	       "\n"
	       "ps: commit is required for write cmds, so -f or stdin should be used\n"
	       "    setting env UTIL_DBG to 1 will enable debug msg in detail\n");
}

int
main(int argc, char *argv[])
{
	unsigned int session_expire;
	int ret;
	char *line = NULL;
	size_t len = 0;
	ssize_t lineSize = 0;
	FILE *input = stdin;
	UCI_LINE uci_line;
	char util_usr[]="trex", util_pwd[]="trex1001";
	const char *dbg_str = getenv("UTIL_DBG");
	char *usr_str = getenv("UTIL_USR");
	char *pwd_str = getenv("UTIL_PWD");

	if (dbg_str) {
		dbg_level = atoi(dbg_str);
		//printf("dbg_level=%d\n",dbg_level);
	}
	if (pwd_str==NULL) {
		pwd_str=util_pwd;
	}
	if (usr_str==NULL) {
		usr_str=util_usr;
	}
// CLIENT INIT
	dbprintf_wanif("util_ubus_client_init\n");
	if (util_ubus_client_init() != 0)
		return -1;
	dbprintf_wanif("util_ubus_session_login\n");
// SESSSION_LOGIN
	// login with session idle timeout 60s
	ret = util_ubus_session_login(usr_str, pwd_str, 60);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
// SESSION_VALIDATE, ret != 0 if session is invalid, session_expire means seconds to expire the rpcd session
	dbprintf_wanif("util_ubus_session_validate\n");
	ret = util_ubus_session_validate(&session_expire);
	dbprintf_wanif("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));

	if (argc > 1) {
		if (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "-?", 2) == 0) {
			print_help();
			return 0;
		} else if (strncmp(argv[1], "-f", 2) == 0) {
			if (argc > 2) {
				if (!(input = fopen(argv[2], "r"))) {
					printf("open %s error!\n", argv[2]);
					util_ubus_client_finish();
					return -1;
				}
			}
		} else {
			line = malloc(256);
			if (!line)
				return -1;
			memset(line, 0, 256);
			for (int i = 1; i < argc; i++) {
				strcat(line, argv[i]);
				strcat(line, " ");
			}
			line[strlen(line) - 1] = 0;
			strcat(line, "\n");
			uci_line_init(&uci_line, 0);
			uci_line_parser(line, &uci_line);
			if (dbg_level)
				uci_line_dump(&uci_line);
			ret = uci_run(&uci_line);
			dbprintf_wanif("uci_run ret=%d\n", ret);

			if (uci_line.cmd_idx != UCI_CMD_GET &&
				uci_line.cmd_idx != UCI_CMD_CHANGES &&
				uci_line.cmd_idx != UCI_CMD_COMMIT &&
				uci_line.cmd_idx != UCI_CMD_RESTART &&
				uci_line.cmd_idx != UCI_CMD_RENEW &&
				uci_line.cmd_idx != UCI_CMD_RELOAD) {
				ret = uci_cmd_commit(&uci_line);
				dbprintf_wanif("uci_cmd_commit ret=%d\n", ret);
			}
			uci_line_init(&uci_line, 1);
			free(line);
			line = NULL;
			util_ubus_client_finish();
			return 0;
		}
	}

	uci_line_init(&uci_line, 0);
	while (!feof(input)) {
		lineSize = getline(&line, &len, input);

		if (lineSize == 1)
			dbprintf_wanif("char =%x\n", line[0]);
		else if (lineSize > 1 && line[0] != '#') {
			uci_line_parser(line, &uci_line);
			if (dbg_level)
				uci_line_dump(&uci_line);
			ret = uci_run(&uci_line);
			dbprintf_wanif("ret=%d\n", ret);
			uci_line_init(&uci_line, 1);
		}
		if (line) {
			free(line);
			line = NULL;
			len = 0;
		}

	}

	util_ubus_client_finish();
	if (input != stdin)
		fclose(input);
	if (configName)
		free(configName);

	return 0;
}
