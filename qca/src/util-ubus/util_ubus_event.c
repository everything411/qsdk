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

#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>

#define UBUS_EVENT_CTL			"util_ubus_event_ctl"
//#define UBUS_EVENT_REMOVE_DEVICE      "rm_device"
#define UBUS_EVENT_ALL			"*"
#define UBUS_EVENT_NETWORK		"network.interface"
#define RULEFILE			"/etc/omci/util_event_rule.cfg"

static inline char *
util_purename(char *path)
{
	char *delim = rindex(path, '/');
	return delim ? (delim + 1) : path;
}

#define dbprintf(fmt, args...)      do { if(dbg_level) fprintf(stderr,"%s:%d: %s(): " fmt, util_purename(__FILE__), __LINE__, __FUNCTION__, ##args); } while (0)

typedef struct __ubus_event_t__ {
	char event_name[128];
	char shell_name[256];
	char cond_result;
	struct list_head cond_list;
	struct list_head event_node;
} UBUS_EVENT_T;

typedef struct __cond_list_t__ {
	char cond_patten[64];
	char cond_value[64];
	char ismatch;
	char operate;		//'&','|','\0'
	struct list_head cond_node;
} COND_L_T;

typedef struct __event_head_list__ {
	int list_cnt;
	char event_name[128];
	struct list_head event_head_list;
	struct list_head event_head_node;
} EVENT_HEAD_LIST;

static struct ubus_context *ctx = NULL;
static const char *cli_path;
static int shutdown = 0, shell_run = 0;
static int trap_mode = -1, single_mode = -1;
static char ifname[16] = { 0 };
static char script_name[256] = { 0 };

int dbg_level = 0, cfg_reload = 0, cfg_dump = 0;
struct list_head eventlist;
int timer_delay = 5;
int shell_delay = 5;
static struct uloop_timeout timer_shutdown;
static struct uloop_timeout timer_shell;
char rulefile[128] = RULEFILE;

int
init_event_head_inst(EVENT_HEAD_LIST ** inst)
{
	*inst = malloc(sizeof(EVENT_HEAD_LIST));
	if (*inst == NULL)
		return -1;
	memset(*inst, 0, sizeof(EVENT_HEAD_LIST));
	INIT_LIST_HEAD(&(*inst)->event_head_list);
	INIT_LIST_HEAD(&(*inst)->event_head_node);
	return 0;
}

int
init_ubus_event_inst(UBUS_EVENT_T ** inst)
{
	*inst = malloc(sizeof(UBUS_EVENT_T));
	if (*inst == NULL)
		return -1;
	memset(*inst, 0, sizeof(UBUS_EVENT_T));
	INIT_LIST_HEAD(&(*inst)->cond_list);
	INIT_LIST_HEAD(&(*inst)->event_node);
	return 0;
}

int
init_cond_inst(COND_L_T ** inst)
{
	*inst = malloc(sizeof(COND_L_T));
	if (*inst == NULL)
		return -1;
	memset(*inst, 0, sizeof(COND_L_T));
	INIT_LIST_HEAD(&(*inst)->cond_node);
	return 0;
}

int
destory_eventlist(struct list_head *listinst)
{
	UBUS_EVENT_T *ubus_event_inst, *n1;
	COND_L_T *cond_inst, *n2;
	EVENT_HEAD_LIST *event_head, *n0;
	list_for_each_entry_safe(event_head, n0, listinst, event_head_node) {
		list_for_each_entry_safe(ubus_event_inst, n1, &event_head->event_head_list, event_node) {
			list_for_each_entry_safe(cond_inst, n2, &ubus_event_inst->cond_list, cond_node) {
				list_del(&cond_inst->cond_node);
				free(cond_inst);
			}
			list_del(&ubus_event_inst->event_node);
			free(ubus_event_inst);
		}
		list_del(&event_head->event_head_node);
		free(event_head);
	}
	memset(listinst, 0, sizeof(struct list_head));
	return 0;
}

int
dump_eventlist(struct list_head *listinst)
{
	UBUS_EVENT_T *ubus_event_inst;
	COND_L_T *cond_inst;
	EVENT_HEAD_LIST *event_head;
	int i;
	list_for_each_entry(event_head, listinst, event_head_node) {
		i = 0;
		printf("event_name:%s, list_cnt=%d\n", event_head->event_name, event_head->list_cnt);
		list_for_each_entry(ubus_event_inst, &event_head->event_head_list, event_node) {
			printf("(%d)event_name: %s, shell: %s\n", i, ubus_event_inst->event_name, ubus_event_inst->shell_name);
			list_for_each_entry(cond_inst, &ubus_event_inst->cond_list, cond_node) {
				printf("%s=%s", cond_inst->cond_patten, cond_inst->cond_value);
				if (cond_inst->operate)
					printf(" %c ", cond_inst->operate);
				else
					printf("\n");
			}
			i++;
		}
		printf("\n");
	}
	return 0;
}

int
set_ubus_event_inst(UBUS_EVENT_T * inst, char *event, char *shell)
{
	strncpy(inst->event_name, event, 127);
	inst->event_name[127] = 0;
	strncpy(inst->shell_name, shell, 255);
	inst->shell_name[255] = 0;
	return 0;
}

int
set_cond_inst(COND_L_T * inst, char *patten, char *value, char oper)
{
	strncpy(inst->cond_patten, patten, 63);
	inst->cond_patten[63] = 0;
	strncpy(inst->cond_value, value, 63);
	inst->cond_value[63] = 0;
	if (oper == '&' || oper == '|')
		inst->operate = oper;
	else
		inst->operate = 0;
	return 0;
}

EVENT_HEAD_LIST *
search_event_head_node(char *event_name)
{
	EVENT_HEAD_LIST *event_head;

	list_for_each_entry(event_head, &eventlist, event_head_node) {
		if (strcmp(event_head->event_name, event_name) == 0) {
			return event_head;
		}
	}
	return NULL;
}

int
parser_input_rule(char *line)
{
	char *tmp;
	char *patten, *shell, *condstr, *value, *oper;
	UBUS_EVENT_T *ubus_event;
	COND_L_T *cond;
	EVENT_HEAD_LIST *event_head;

	tmp = strtok(line, "\n");
//get patten string
	if ((tmp = strchr(line, ',')) == NULL)
		return -1;
	*tmp = 0;
	patten = line;
//get shell string
	shell = ++tmp;
	if ((tmp = strchr(shell, ',')) == NULL)
		return -1;
	*tmp = 0;
	if (*shell == 0)
		return -1;
//create & set ubus_event instance
	init_ubus_event_inst(&ubus_event);
	set_ubus_event_inst(ubus_event, patten, shell);
//parser condition string
	condstr = ++tmp;
	patten = strtok(condstr, " ");
	while (patten != NULL) {
		init_cond_inst(&cond);
		tmp = strchr(patten, '=');
		if (tmp != NULL) {
			*tmp = 0;
			if (*(tmp + 1) != 0) {
				value = tmp + 1;
			} else {
				*value = 0;
			}
			oper = strtok(NULL, " ");
			if (oper != NULL) {
				set_cond_inst(cond, patten, value, *oper);
				list_add_tail(&cond->cond_node, &ubus_event->cond_list);
			} else {
				set_cond_inst(cond, patten, value, '\0');
				list_add_tail(&cond->cond_node, &ubus_event->cond_list);
				break;
			}
		}
		patten = strtok(NULL, " ");
	}
	event_head = search_event_head_node(ubus_event->event_name);
	if (event_head) {	//add to exist node
		list_add_tail(&ubus_event->event_node, &event_head->event_head_list);
		event_head->list_cnt++;
	} else {		//create new node and add to eventlist
		init_event_head_inst(&event_head);
		strncpy(event_head->event_name, ubus_event->event_name, 127);
		event_head->event_name[127] = 0;
		list_add_tail(&ubus_event->event_node, &event_head->event_head_list);
		event_head->list_cnt++;
		list_add_tail(&event_head->event_head_node, &eventlist);
	}
	return 0;
}

int
read_rule_file(char *filename)
{
	FILE *fp;
	char *line, buff[256];
	size_t len = sizeof(buff) - 1;
	ssize_t lineSize = 0;

	INIT_LIST_HEAD(&eventlist);
	line = buff;
	fp = fopen(filename, "r");
	if (fp == NULL)
		return -1;
	while (!feof(fp)) {
		lineSize = getline(&line, &len, fp);
		if (lineSize > 1 && line[0] != '#' && line[0] != ',') {
			parser_input_rule(line);
		}
	}
	fclose(fp);
#if 0
	dump_eventlist(&eventlist);
	destory_eventlist(&eventlist);

	if (list_empty(&eventlist))
		printf("eventlist is empty\n");
#endif
	return 0;
}

static void
shutdown_timer_cb(struct uloop_timeout *timeout)
{
	//uloop_timeout_cancel(timeout);
	uloop_cancelled = true;
}

#if 1
static void
shell_timer_cb(struct uloop_timeout *timeout)
{
	//char cmd[128]={0};

	//snprintf(cmd, 127, "ifname=%s trap_mode=%d single_mode=%d %s",
	//                ifname, trap_mode, single_mode,script_name);

	dbprintf("call script: %s\n", script_name);
	system(script_name);
	if (shell_run)
		uloop_timeout_set(&timer_shell, shell_delay * 1000);
}
#endif

int
ubus_get_objstr(struct blob_attr *msg, char *key, char *str, int strlen)
{
	int ret = 0;
	json_object *jobj;
	enum json_type jtype;
	json_object *obj_key;
	const char *strval;

	jobj = json_tokener_parse(blobmsg_format_json(msg, true));
	if(NULL == jobj){
		dbprintf("parse error\n");
		return -1;
	}
	if(!json_object_is_type(jobj, json_type_object)) {
		json_object_put(jobj);
		dbprintf("error json string\n");
		return -1;
	}
	if (json_object_object_get_ex(jobj, key, &obj_key)) {
		jtype = json_object_get_type(obj_key);
		switch (jtype) {
		case json_type_string:
			strval = json_object_get_string(obj_key);
			dbprintf("%s=%s\n", key, strval);
			*str = 0;
			strncpy(str, strval, strlen);
			break;
		default:
			ret = -1;
			break;
		}
	} else
		ret = -1;
	json_object_put(jobj);
	return ret;
}

int
ubus_get_objint(struct blob_attr *msg, char *key, int *value)
{
	int ret = 0;
	json_object *jobj;
	enum json_type jtype;
	json_object *obj_key;

	jobj = json_tokener_parse(blobmsg_format_json(msg, true));
	if(NULL == jobj){
		dbprintf("parse error\n");
		return -1;
	}
	if(!json_object_is_type(jobj, json_type_object)) {
		json_object_put(jobj);
		dbprintf("error json string\n");
		return -1;
	}
	if (json_object_object_get_ex(jobj, key, &obj_key)) {
		jtype = json_object_get_type(obj_key);
		switch (jtype) {
		case json_type_int:
			*value = json_object_get_int(obj_key);
			dbprintf("%s=%d\n", key, *value);
			break;
		default:
			ret = -1;
			break;
		}
	} else {
		ret = -1;
	}
	json_object_put(jobj);
	return ret;
}

static void
ubus_probe_device_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg)
{
	EVENT_HEAD_LIST *event_head;
	UBUS_EVENT_T *ubus_event_inst;
	COND_L_T *cond_inst;
	char *str;
	const char *strval;
	json_object *jobj;
	json_object *obj_key;
	char oper;
	char pre_param[128] = { 0 }, param[128] = {
	0}, run_cmd[1025] = {
	0};

	if (!msg )
		return;
	str = blobmsg_format_json(msg, true);
	dbprintf("{ \"%s\": %s }\n", type, str);
	if (strncmp(type, UBUS_EVENT_CTL, strlen(UBUS_EVENT_CTL)) == 0) {
		dbprintf("ubus ctl\n");
		ubus_get_objint(msg, "set_delay", &timer_delay);
		if (!ubus_get_objint(msg, "shutdown", &shutdown)) {
			if (shutdown)
				uloop_timeout_set(&timer_shutdown, timer_delay * 1000);
		}
		ubus_get_objint(msg, "shell_delay", &shell_delay);
		if (!ubus_get_objint(msg, "shell_run", &shell_run)) {
			if (shell_run)
				uloop_timeout_set(&timer_shell, shell_delay * 1000);
		}
		ubus_get_objstr(msg, "script", script_name, sizeof(script_name) - 1);
		ubus_get_objstr(msg, "ifname", ifname, sizeof(ifname) - 1);
		ubus_get_objint(msg, "trap_mode", &trap_mode);
		ubus_get_objint(msg, "single_mode", &single_mode);
		ubus_get_objint(msg, "dbg_level", &dbg_level);
		ubus_get_objint(msg, "cfg_reload", &cfg_reload);
		ubus_get_objint(msg, "cfg_dump", &cfg_dump);
		if (cfg_reload) {
			destory_eventlist(&eventlist);
			if (read_rule_file(rulefile) == -1) {
				uloop_cancelled = true;
			}
			dbprintf("Rule file reloaded!\n");
			cfg_reload = 0;
		}
		if (cfg_dump) {
			dump_eventlist(&eventlist);
			cfg_dump = 0;
		}
	}
	jobj = json_tokener_parse(str);
	if(NULL == jobj){
		dbprintf("parse error\n");
		free(str);
		return;
	}
	if(!json_object_is_type(jobj, json_type_object)) {
		json_object_put(jobj);
		dbprintf("error json string\n");
		free(str);
		return;
	}
	list_for_each_entry(event_head, &eventlist, event_head_node) {
		if (strcmp(type, event_head->event_name) == 0) {
			ubus_get_objstr(msg, "pre_param", pre_param, sizeof(pre_param) - 1);
			ubus_get_objstr(msg, "param", param, sizeof(param) - 1);
			list_for_each_entry(ubus_event_inst, &event_head->event_head_list, event_node) {
				ubus_event_inst->cond_result = 0;
				list_for_each_entry(cond_inst, &ubus_event_inst->cond_list, cond_node) {
					cond_inst->ismatch = 0;
					if (json_object_object_get_ex(jobj, cond_inst->cond_patten, &obj_key)) {
						strval = json_object_get_string(obj_key);
						if (strcmp(strval, cond_inst->cond_value) == 0) {
							cond_inst->ismatch = 1;
						}
					}
				}
				oper = 'S';
				list_for_each_entry(cond_inst, &ubus_event_inst->cond_list, cond_node) {
					if (oper == 0) {
						break;
					} else if (oper == 'S') {
						ubus_event_inst->cond_result = cond_inst->ismatch;
					} else {
						if (oper == '&') {
							ubus_event_inst->cond_result &= cond_inst->ismatch;
							if (ubus_event_inst->cond_result)
								break;
						} else if (oper == '|') {
							ubus_event_inst->cond_result |= cond_inst->ismatch;
							if (ubus_event_inst->cond_result)
								break;
						}
					}
					oper = cond_inst->operate;
				}
				if (ubus_event_inst->cond_result) {
					snprintf(run_cmd, sizeof(run_cmd) - 1, "%s %s %s", pre_param,
						 ubus_event_inst->shell_name, param);
					dbprintf("%s\n", run_cmd);
					system(run_cmd);
				}
			}
		}
	}
	json_object_put(jobj);
	free(str);
}

static int
client_ubus_register_events()
{
	static struct ubus_event_handler listener;
	int ret = 0;
	EVENT_HEAD_LIST *event_head;

	memset(&listener, 0, sizeof(listener));
	listener.cb = ubus_probe_device_event;
	list_for_each_entry(event_head, &eventlist, event_head_node) {
		ret = ubus_register_event_handler(ctx, &listener, event_head->event_name);
		if (ret) {
			fprintf(stderr, "register event %s failed.\n", event_head->event_name);
			return -1;
		}
	}

	//for util_ubus_event parameter setting
	ret = ubus_register_event_handler(ctx, &listener, UBUS_EVENT_CTL);
	if (ret) {
		ubus_unregister_event_handler(ctx, &listener);
		fprintf(stderr, "register event failed.\n");
		return -1;
	}
	return 0;
}

static void
ubus_add_fd(void)
{
	ubus_add_uloop(ctx);

#ifdef FD_CLOEXEC
	fcntl(ctx->sock.fd, F_SETFD, fcntl(ctx->sock.fd, F_GETFD) | FD_CLOEXEC);
#endif
}

static void
ubus_reconn_timer(struct uloop_timeout *timeout)
{
	static struct uloop_timeout retry = {
		.cb = ubus_reconn_timer,
	};
	int t = 2;

	if (ubus_reconnect(ctx, cli_path) != 0) {
		uloop_timeout_set(&retry, t * 1000);
		return;
	}
	ubus_add_fd();
}

static void
ubus_connection_lost(struct ubus_context *ctx)
{
	ubus_reconn_timer(NULL);
}

static int
client_ubus_init(const char *path)
{
	uloop_init();
	cli_path = path;

	ctx = ubus_connect(path);
	if (!ctx) {
		printf("ubus connect failed\n");
		return -1;
	}

	printf("connected as %08x\n", ctx->local_id);
	ctx->connection_lost = ubus_connection_lost;

	ubus_add_fd();

	return 0;
}

static void
client_ubus_done(void)
{
	if (ctx)
		ubus_free(ctx);
}

int
main(int argc, char *argv[])
{
	char *path = NULL;
	const char *dbg_str = getenv("UTIL_DBG");
	if (dbg_str) {
		dbg_level = atoi(dbg_str);
		//printf("dbg_level=%d\n",dbg_level);
	}

	if (UBUS_STATUS_OK != client_ubus_init(path)) {
		printf("ubus connect failed!\n");
		return -1;
	}
	if (argc > 1)
		strncpy(rulefile, argv[1], sizeof(rulefile) - 1);
	if (read_rule_file(rulefile) == -1) {
		printf("load %s failed!\n",rulefile);
		client_ubus_done();
		return -1;
	}
	//init variable
	strcpy(ifname, "eth4");
	strcpy(script_name, "network_cb.sh");
	trap_mode = -1;
	single_mode = -1;
	client_ubus_register_events();
	timer_shutdown.cb = shutdown_timer_cb;
	timer_shell.cb = shell_timer_cb;
	uloop_run();
	client_ubus_done();
	destory_eventlist(&eventlist);
	return 0;
}
