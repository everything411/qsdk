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

static int util_ubus_debug = UTIL_UBUS_DEBUG_DEFAULT;
static int ubus_invoke_timeout = UBUS_INVOKE_TIMEOUT_DEFAULT;
static struct ubus_context *ubus_ctx = NULL;
static char rpc_session_id[129];

// common routines //////////////////////////////////////////////////////////////////////////////////////////
int
util_ubus_client_init(void)
{
	char *path = NULL;	// ipc_path: unix domain socket filename, NULL means default /var/run/ubus.sock
	//uloop_init();
	ubus_ctx = ubus_connect(path);
	if (!ubus_ctx) {
		dbprintf("ubus connect failed\n");
		return -1;
	}
	if (util_ubus_debug)
		dbprintf("connected as %08x\n", ubus_ctx->local_id);

	return 0;
}

void
util_ubus_client_finish(void)
{
	if (ubus_ctx)
		ubus_free(ubus_ctx);
	//uloop_done();
}

static void
util_ubus_dump_blobmsg(struct blob_attr *msg)
{
	static bool simple_output = false;
	char *str = blobmsg_format_json_indent(msg, true, simple_output ? -1 : 0);
	dbprintf("\n%s\n", str);
	free(str);
}

char *
util_ubus_error_str(int error)
{
	switch (error) {
		// ubus system error            
	case UBUS_STATUS_OK:
		return "ok";
	case UBUS_STATUS_INVALID_COMMAND:
		return "invalid cmd";
	case UBUS_STATUS_INVALID_ARGUMENT:
		return "invalid arg";
	case UBUS_STATUS_METHOD_NOT_FOUND:
		return "method not found";
	case UBUS_STATUS_NOT_FOUND:
		return "not found";
	case UBUS_STATUS_NO_DATA:
		return "no data";
	case UBUS_STATUS_PERMISSION_DENIED:
		return "permission denied";
	case UBUS_STATUS_TIMEOUT:
		return "timeout";
	case UBUS_STATUS_NOT_SUPPORTED:
		return "not supported";
	case UBUS_STATUS_UNKNOWN_ERROR:
		return "unknown err";
	case UBUS_STATUS_CONNECTION_FAILED:
		return "connection failed";
		// util_ubus_cb_error
	case UTIL_UBUS_ERROR_NULL_POINTER:
		return "null_pointer";
	case UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL:
		return "value_buffer_too_small";
	case UTIL_UBUS_ERROR_ENTRY_NOT_FOUND:
		return "entry_not_found";
	case UTIL_UBUS_ERROR_BLOB_MSG_NULL:
		return "blob_msg_null";
	case UTIL_UBUS_ERROR_BLOB_MSG_PARSE:
		return "blob_msg_parse_error";
	case UTIL_UBUS_ERROR_BLOB_ATTR_NOT_FOUND:
		return "blob_attr_not_found";
	case UTIL_UBUS_ERROR_BLOB_ATTR_PARSE:
		return "blob_attr_parse_error";
	case UTIL_UBUS_ERROR_BLOB_MSG_TYPE_UNSUPPORTED:
		return "blob_msg_type_unsupported";

	case UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_NAME:
		return "can_not_find_related_key_name";
	case UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_VALUE:
		return "can_not_find_related_key_value";
	case UTIL_ERROR_BUFFER_FOR_RETURN_IS_TOO_SMALL:
		return "buffer_for_return_is_too_small";

	case UTIL_UBUS_OK_ENTRY_FOUND:
		return "entry_found";
	}
	return "?";
}

char *
util_ubus_blogmsg_type_str(unsigned int type)
{
	switch (type) {
	case BLOBMSG_TYPE_UNSPEC:
		return "unspec";
	case BLOBMSG_TYPE_ARRAY:
		return "array";
	case BLOBMSG_TYPE_TABLE:
		return "table";
	case BLOBMSG_TYPE_STRING:
		return "string";
	case BLOBMSG_TYPE_INT64:
		return "int64";
	case BLOBMSG_TYPE_INT32:
		return "int32";
	case BLOBMSG_TYPE_INT16:
		return "int16";
	case BLOBMSG_TYPE_INT8:
		return "int8";
	}
	return "?";
}

// session login /////////////////////////////////////////////////////////////////////////////////////////

enum {
	SESSION_LOGIN_SID,
	SESSION_LOGIN_TIMEOUT,
	SESSION_LOGIN_EXPIRES,
	SESSION_LOGIN_ACLS,
	SESSION_LOGIN_DATA,
	__SESSION_LOGIN_MAX,
};
static const struct blobmsg_policy
 session_login_reply_policy[__SESSION_LOGIN_MAX] = {
	[SESSION_LOGIN_SID] = {.name = "ubus_rpc_session",.type = BLOBMSG_TYPE_STRING},
	[SESSION_LOGIN_TIMEOUT] = {.name = "timeout",.type = BLOBMSG_TYPE_INT32},
	[SESSION_LOGIN_EXPIRES] = {.name = "expires",.type = BLOBMSG_TYPE_UNSPEC},
	[SESSION_LOGIN_ACLS] = {.name = "acls",.type = BLOBMSG_TYPE_TABLE},
	[SESSION_LOGIN_DATA] = {.name = "data",.type = BLOBMSG_TYPE_TABLE},
};

struct util_ubus_session_login_priv_t {
	char sid[129];
	int ret;
};

static void
util_ubus_session_login_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct util_ubus_session_login_priv_t *priv = req->priv;
	struct blob_attr *tb[__SESSION_LOGIN_MAX];
	char *sid;
	int ret;

	if (!msg) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_NULL;
		return;
	}
	//if (util_ubus_debug)
	//      util_ubus_dump_blobmsg(msg);

	if ((ret =
	     blobmsg_parse(session_login_reply_policy, __SESSION_LOGIN_MAX, tb, blobmsg_data(msg),
			   blobmsg_data_len(msg))) != 0) {
		dbprintf("blobmsg_parse uci error %d\n", ret);
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_PARSE;
		return;
	}
	if (!tb[SESSION_LOGIN_SID]) {	// blob_attr exist?
		util_ubus_dump_blobmsg(msg);
		priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_NOT_FOUND;
		return;
	}
	if ((sid = blobmsg_get_string(tb[SESSION_LOGIN_SID])) == NULL) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
		return;
	}
	strncpy(priv->sid, sid, 128);	// we ensure priv->sid[129] is enough for sid(64byte)
	priv->ret = 0;
	if (util_ubus_debug)
		dbprintf("sid=%s\n", sid);
}

// if session_timeout is 0, caller has to call util_ubus_destroy to clean up session
// if session_timeout n > 0, session would be disabled if session is idle for n seconds
int
util_ubus_session_login(char *username, char *password, unsigned int session_timeout)
{
	unsigned int ubus_path_id;
	struct blob_buf b;
	int ret;
	struct util_ubus_session_login_priv_t priv;

	if (!username || !password)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "username", username);
	blobmsg_add_string(&b, "password", password);
	blobmsg_add_u32(&b, "timeout", session_timeout);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "session", &ubus_path_id)) != 0) {
		dbprintf("lookup 'session' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	memset(&priv, 0, sizeof(priv));
	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "login", b.head, util_ubus_session_login_cb, &priv,
			 ubus_invoke_timeout * 1000)) != 0) {
		dbprintf("ubus_invoke session::login error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}
	blob_buf_free(&b);

	rpc_session_id[0] = 0;
	if (priv.ret < 0) {
		dbprintf("ubus_invoke session::login cb error %d(%s)\n", priv.ret, util_ubus_error_str(priv.ret));
		return priv.ret;
	}
	strncpy(rpc_session_id, priv.sid, 128);
	return 0;
}

// session validate /////////////////////////////////////////////////////////////////////////////////

struct util_ubus_session_validate_priv_t {
	unsigned int expire;
	int ret;
};

static void
util_ubus_session_validate_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct util_ubus_session_validate_priv_t *priv = req->priv;
	struct blob_attr *tb[__SESSION_LOGIN_MAX];
	char *sid;
	int ret;

	if (!msg) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_NULL;
		return;
	}
	//if (util_ubus_debug)
	//      util_ubus_dump_blobmsg(msg);

	if ((ret =
	     blobmsg_parse(session_login_reply_policy, __SESSION_LOGIN_MAX, tb, blobmsg_data(msg),
			   blobmsg_data_len(msg))) != 0) {
		dbprintf("blobmsg_parse uci error %d\n", ret);
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_PARSE;
		return;
	}
	if (!tb[SESSION_LOGIN_SID] || !tb[SESSION_LOGIN_EXPIRES]) {	// blob_attr exists?
		util_ubus_dump_blobmsg(msg);
		priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_NOT_FOUND;
		return;
	}
	if ((sid = blobmsg_get_string(tb[SESSION_LOGIN_SID])) == NULL) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
		return;
	}
	priv->expire = blobmsg_get_u32(tb[SESSION_LOGIN_EXPIRES]);
	priv->ret = 0;
	if (util_ubus_debug)
		dbprintf("sid=%s, expire time=%u\n", sid, priv->expire);
}

int
util_ubus_session_validate(unsigned int *session_expire)
{
	unsigned int ubus_path_id;
	struct blob_buf b;
	int ret;
	struct util_ubus_session_validate_priv_t priv = {.expire = 0,.ret = UTIL_UBUS_ERROR_ENTRY_NOT_FOUND };

	if (!session_expire)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "session", &ubus_path_id)) != 0) {
		dbprintf("lookup 'session' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "list", b.head, util_ubus_session_validate_cb, &priv,
			 ubus_invoke_timeout * 1000)) != 0) {
		dbprintf("ubus_invoke session::list error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}
	blob_buf_free(&b);

	*session_expire = 0;
	if (priv.ret < 0) {
		dbprintf("ubus_invoke session::validate cb error %d(%s)\n", priv.ret, util_ubus_error_str(priv.ret));
		return priv.ret;
	}
	*session_expire = priv.expire;
	return 0;
}

// session destroy /////////////////////////////////////////////////////////////////////////////////

static void
util_ubus_session_destroy_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

int
util_ubus_session_destroy(void)
{
	unsigned int ubus_path_id;
	struct blob_buf b;
	int ret;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "session", &ubus_path_id)) != 0) {
		dbprintf("lookup 'session' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}
	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "destroy", b.head, util_ubus_session_destroy_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0) {
		dbprintf("ubus_invoke session::destroy error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}
	blob_buf_free(&b);

	rpc_session_id[0] = 0;
	return 0;
}

// uci get /////////////////////////////////////////////////////////////////////////////////////////

struct util_ubus_uci_get_priv_t {
	unsigned int value_size;
	char *value;
	int ret;
};
static void
util_ubus_uci_get_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct util_ubus_uci_get_priv_t *priv = req->priv;
	char *value;
	struct blob_attr *cur;
	int rem, found = 0;

	if (!msg) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_NULL;
		return;
	}
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);

	blobmsg_for_each_attr(cur, msg, rem) {
		unsigned int type = blobmsg_type(cur);

		if (type == BLOBMSG_TYPE_STRING) {
			if ((value = blobmsg_get_string(cur)) == NULL) {
				priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
				return;
			}
			strncat(priv->value, value, priv->value_size - 1);
			found++;

		} else if (type == BLOBMSG_TYPE_ARRAY) {
			struct blob_attr *_cur;
			int _rem;
			blobmsg_for_each_attr(_cur, cur, _rem) {
				unsigned int _type = blobmsg_type(_cur);
				if (_type == BLOBMSG_TYPE_STRING) {
					int len;
					if ((value = blobmsg_get_string(_cur)) == NULL) {
						priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
						continue;
					}
					if ((len = strlen(priv->value)) > 0) {
						if (priv->value_size - 1 - len <= 2) {
							priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
							return;
						}
						strncat(priv->value, " ", priv->value_size - 1 - len);
						len += 1;
					}
					if (priv->value_size - 1 - len < strlen(value)) {
						priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
						return;
					}
					strncat(priv->value, value, priv->value_size - 1 - len);
					found++;
				} else {
					if (util_ubus_debug)
						dbprintf("2nd layer: unsupported  blobmsg_type %u(%s)\n", _type,
							 util_ubus_blogmsg_type_str(_type));
					priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_TYPE_UNSUPPORTED;
				}
			}
		} else if (type == BLOBMSG_TYPE_TABLE) {
			//section
			struct blob_attr *_cur;
			int _rem;

			blobmsg_for_each_attr(_cur, cur, _rem) {
				unsigned int _type = blob_id(_cur);
				const struct blobmsg_hdr *hdr;

				//maybe useful in some case
				//if (!blob_is_extended(attr))
				//continue;

				/*
				   if the section is exist, we can find option like ".name": "cfg03f65c"
				 */
				hdr = blob_data(_cur);
				if (_type == BLOBMSG_TYPE_STRING) {
					int len;

					if (util_ubus_debug >= 3)
						dbprintf("type=%u(%s) name=%s\n", type, util_ubus_blogmsg_type_str(_type),
							 (char *)hdr->name);

					if ((value = blobmsg_get_string(_cur)) == NULL) {
						priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
						continue;
					}
					//only keep .type's value???
					//if (memcmp(hdr->name, ".type", 5) != 0) {
					if (memcmp(hdr->name, ".anonymous", 10) == 0 || memcmp(hdr->name, ".name", 5) == 0) {
						continue;
					}

					if ((len = strlen(priv->value)) > 0) {
						if (priv->value_size - 1 - len <= 2) {
							priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
							return;
						}
						strncat(priv->value, ",", priv->value_size - 1 - len);
						len += 1;
					}
					if (priv->value_size - 1 - len < strlen(value) + strlen((char *)hdr->name) + 1) {
						priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
						return;
					}
					strncat(priv->value, (char *)hdr->name, priv->value_size - 1 - strlen(priv->value));
					strncat(priv->value, ":", priv->value_size - 1 - strlen(priv->value));
					strncat(priv->value, value, priv->value_size - 1 - strlen(priv->value));
					found++;

				} else if (_type == BLOBMSG_TYPE_INT64 || _type == BLOBMSG_TYPE_INT32 ||
					   _type == BLOBMSG_TYPE_INT16 || _type == BLOBMSG_TYPE_INT8) {
					if (util_ubus_debug)
						dbprintf("type=%u(%s)\n", type, util_ubus_blogmsg_type_str(_type));

				} else if (_type == BLOBMSG_TYPE_BOOL) {
					if (util_ubus_debug)
						dbprintf("type=%u(%s)\n", type, util_ubus_blogmsg_type_str(_type));

				} else if (_type == BLOBMSG_TYPE_ARRAY) {
					struct blob_attr *_cur1;
					int _rem1, len1 = 0, list_cnt = 0;
					hdr = blob_data(_cur);
					if ((len1 = strlen(priv->value)) > 0) {
						if (priv->value_size - 1 - len1 <= 2) {
							priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
							return;
						}
						strncat(priv->value, ",", priv->value_size - 1 - len1);
					}
					strncat(priv->value, (char *)hdr->name, priv->value_size - 1 - strlen(priv->value));
					strncat(priv->value, ":", priv->value_size - 1 - strlen(priv->value));
					blobmsg_for_each_attr(_cur1, _cur, _rem1) {
						if ((value = blobmsg_get_string(_cur1)) == NULL) {
							priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
							continue;
						}
						if ((len1 = strlen(priv->value)) > 0) {
							if (priv->value_size - 1 - len1 <= 2) {
								priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
								return;
							}
							if (list_cnt > 0) {
								strncat(priv->value, " ", priv->value_size - 1 - len1);
								len1 += 1;
							}
						}
						if (priv->value_size - 1 - len1 < strlen(value) + 1) {
							priv->ret = UTIL_UBUS_ERROR_VALUE_BUFFER_TOO_SMALL;
							return;
						}

						strncat(priv->value, value, priv->value_size - 1 - len1);
						found++;
						list_cnt++;
						if (util_ubus_debug)
							dbprintf("type=%u(%s)\n", type, util_ubus_blogmsg_type_str(_type));
					}
				} else {
					if (util_ubus_debug)
						dbprintf("2nd layer: unsupported  blobmsg_type %u(%s)\n", _type,
							 util_ubus_blogmsg_type_str(_type));
					priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_TYPE_UNSUPPORTED;
				}
			}
		} else {
			if (util_ubus_debug)
				dbprintf("1st layer: unsupported blobmsg_type %u(%s)\n", type,
					 util_ubus_blogmsg_type_str(type));
			priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_TYPE_UNSUPPORTED;
		}
	}

	if (found > 0)
		priv->ret = 0;
	if (found == 0 && priv->ret == 0)
		priv->ret = UTIL_UBUS_ERROR_ENTRY_NOT_FOUND;
}

// ubus call uci get '{"config":"network2", "section":"wan", "option":"ifname", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_get(char *config, char *section, char *option, char *value, unsigned int value_size)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;
	struct util_ubus_uci_get_priv_t priv = {.value_size = value_size,.value = value,.ret =
		    UTIL_UBUS_ERROR_ENTRY_NOT_FOUND };

	if (!config || !section || !value)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "section", section);
	if (option)
		blobmsg_add_string(&b, "option", option);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	value[0] = 0;

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "get", b.head, util_ubus_uci_get_cb, &priv,
			 ubus_invoke_timeout * 1000)) != 0) {
		dbprintf("ubus_invoke uci::get error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}
	blob_buf_free(&b);

	if (priv.ret < 0) {
		printf("uci:%s\n", util_ubus_error_str(priv.ret));
		return priv.ret;
	}
	return 0;
}

// uci set /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci set' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_set_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci set '{"config":"network2", "section":"wan", "values":{"ifname":"eth3"}, "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_set(char *config, char *section, char *option, char *value)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config || !section || !option || !value)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "section", section);
	{
		void *table_values = blobmsg_open_table(&b, "values");
		blobmsg_add_string(&b, option, value);
		blobmsg_close_table(&b, table_values);
	}
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "set", b.head, util_ubus_uci_set_cb, NULL, ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke session::set error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// ubus call uci set '{"config":"network2", "section":"@device[1]", "values":{ "ports":{{"":"10"},{"":"20"},{"":"30"}}}, ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_set_list(char *config, char *section, char *option, char *array[], int array_size)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int i, ret;

	if (!config || !section || !option || !array)
		return UTIL_UBUS_ERROR_NULL_POINTER;
	// uci_set_list wont allow array_size == 0, use uci_delete instead
	if (array_size == 0)
		return util_ubus_uci_delete(config, NULL, section, option);

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "section", section);
	{
		void *table_values = blobmsg_open_table(&b, "values");
		void *array_option = blobmsg_open_array(&b, option);
		for (i = 0; i < array_size; i++)
			blobmsg_add_string(&b, "", array[i]);
		blobmsg_close_table(&b, array_option);
		blobmsg_close_table(&b, table_values);
	}
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "set", b.head, util_ubus_uci_set_cb, NULL, ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke session::set error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci add_section /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci add' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_add_section_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci add '{"config":"network2", "type":"interface", "name":"wan", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
// name is optional, its value could be null
int
util_ubus_uci_add_section(char *config, char *type, char *name)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config || !type)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);
	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "type", type);
	if (name)
		blobmsg_add_string(&b, "name", name);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "add", b.head, util_ubus_uci_add_section_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::add error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci del /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci del' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_delete_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci del '{"config":"network2", "section":"wan", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
// type,section,option is optional, its value could be null
int
util_ubus_uci_delete(char *config, char *type, char *section, char *option)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	if (type)
		blobmsg_add_string(&b, "type", type);
	if (section)
		blobmsg_add_string(&b, "section", section);
	if (option)
		blobmsg_add_string(&b, "option", option);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "delete", b.head, util_ubus_uci_delete_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::delete error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci revert /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci revert' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_revert_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci revert '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_revert(char *config)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "revert", b.head, util_ubus_uci_revert_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::revert error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci commit /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci commit' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_commit_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci commit '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_commit(char *config)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "commit", b.head, util_ubus_uci_commit_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::commit error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci apply /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci apply' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_apply_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci apply '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
//
// when rollback_enable on
// rollback is scheduled at timeout, caller has to call util_ubus_confirm() to cancel this rollback
// when rollback_enable off
// no rollback is scheduled, but caller has to call util_ubus_rollback() manually in case issues found after util_ubus_apply()
int
util_ubus_uci_apply(int rollback_enable, unsigned int auto_rollback_timeout)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_u8(&b, "rollback", rollback_enable);
	blobmsg_add_u32(&b, "timeout", auto_rollback_timeout);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "apply", b.head, util_ubus_uci_apply_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::apply error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci confirm /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci confirm' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_confirm_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci confirm '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_confirm(void)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "confirm", b.head, util_ubus_uci_confirm_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::confirm error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci rollback /////////////////////////////////////////////////////////////////////////////////////////

// it seems the 'uci rollback' wont return meaningful msg, so this callback is not called
static void
util_ubus_uci_rollback_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (!msg)
		return;
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);
}

// ubus call uci rollback '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_rollback(void)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "rollback", b.head, util_ubus_uci_rollback_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::rollback error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

// uci dump_changes /////////////////////////////////////////////////////////////////////////////////////////

static void
util_ubus_uci_dump_changes_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	if (msg) {
		char *str = blobmsg_format_json_indent(msg, true, 0);
		printf("%s\n", str);
		free(str);
	}
}

// ubus call uci changes '{"config":"network2", "ubus_rpc_session":"3dd6cf717b85041e73ad660066577727"}'
int
util_ubus_uci_dump_changes(char *config)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret;

	if (!config)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
	blob_buf_init(&b, 0);

	// prepare parameter
	blobmsg_add_string(&b, "config", config);
	blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

	// check if path 'session' is registered on ubusd
	if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
		dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
		blob_buf_free(&b);
		return ret;
	}

	if ((ret =
	     ubus_invoke(ubus_ctx, ubus_path_id, "changes", b.head, util_ubus_uci_dump_changes_cb, NULL,
			 ubus_invoke_timeout * 1000)) != 0)
		dbprintf("ubus_invoke uci::changes error %d(%s)\n", ret, util_ubus_error_str(ret));
	blob_buf_free(&b);
	return ret;
}

/*
find parent/child by section

char *openwrt_section_relation_file, 	//input
char *child_config, 				//input
char *child_section_type,			//input
char *parent_config, 				//input
char *parent_section_type, 			//input
char *key_name)					//output
*/

static int
find_key_name_by_parent_child_section(char *openwrt_section_relation_file, char *child_config, char *child_section_type,
				      char *parent_config, char *parent_section_type, char *key_name)
{
	FILE *fptr_cfg;
	char buf_line[BUF_LINE_LEN], parent_cfg[TOKEN_LEN], child_cfg[TOKEN_LEN], key_in_child_cfg[TOKEN_LEN];
	char child_input[TOKEN_LEN * 2], parent_input[TOKEN_LEN * 2];

	if (openwrt_section_relation_file == NULL) {
		openwrt_section_relation_file = UTIL_SECTION_RELATION_CONFIGURE;	//"/etc/section_relation.cfg"
	}

	if ((fptr_cfg = fopen(openwrt_section_relation_file, "r")) == NULL) {
		dbprintf("Can' t open file %s !!!\n", openwrt_section_relation_file);
		return -1;
	}
	snprintf(child_input, TOKEN_LEN * 2, "%s.@%s", child_config, child_section_type);
	snprintf(parent_input, TOKEN_LEN * 2, "%s.@%s", parent_config, parent_section_type);

	while (fgets(buf_line, BUF_LINE_LEN, fptr_cfg) != 0) {
		if (sscanf(buf_line, "%s %s %s", parent_cfg, child_cfg, key_in_child_cfg) != 3) {
			dbprintf("invalid input: %s", buf_line);
			fclose(fptr_cfg);
			return -1;
		}

		if (strcmp(parent_input, parent_cfg) == 0 && strcmp(child_input, child_cfg) == 0) {
			//found primary key name
			strncpy(key_name, key_in_child_cfg, TOKEN_LEN);
			if (util_ubus_debug)
				dbprintf("found primary key name: %s\n", key_name);
			return 0;
		}
	}
	return -1;
}

static void
find_ubus_uci_get_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct util_ubus_uci_get_priv_t *priv = req->priv;
	char *value;
	struct blob_attr *cur;
	//int rem, index=0, found=0;
	int rem, found;

	if (!msg) {
		priv->ret = UTIL_UBUS_ERROR_BLOB_MSG_NULL;
		return;
	}
	if (util_ubus_debug)
		util_ubus_dump_blobmsg(msg);

	found = 0;
	blobmsg_for_each_attr(cur, msg, rem) {
		unsigned int type = blobmsg_type(cur);
		if (type == BLOBMSG_TYPE_TABLE) {
			//section
			struct blob_attr *_cur;
			int _rem;

			blobmsg_for_each_attr(_cur, cur, _rem) {
				unsigned int _type = blob_id(_cur);
				const struct blobmsg_hdr *hdr;

				//maybe useful in some case
				//if (!blob_is_extended(attr))
				//continue;

				/*
				   if the section is exist, we can find option like ".name": "cfg03f65c"
				 */
				hdr = blob_data(_cur);
				if (_type == BLOBMSG_TYPE_STRING) {
					//int len;

					if ((value = blobmsg_get_string(_cur)) == NULL) {
						priv->ret = UTIL_UBUS_ERROR_BLOB_ATTR_PARSE;
						continue;
					}
					//only keep .name's value???
					if (memcmp(hdr->name, ".name", 5) != 0) {
						continue;
					}

					if (util_ubus_debug) {
						dbprintf("type=%u(%s) name=%s\n", type, util_ubus_blogmsg_type_str(_type),
							 (char *)hdr->name);
						dbprintf("compare priv->value=%s, value=%s\n", priv->value, value);
					}

					found++;	//found section
					if (strcmp(priv->value, value) == 0) {
						if (util_ubus_debug)
							dbprintf("found section name=%s\n", value);
						priv->ret = UTIL_UBUS_OK_ENTRY_FOUND;
					}
				}
			}
		}
	}

	if (found > 0 && priv->ret != UTIL_UBUS_OK_ENTRY_FOUND)	//still has valid section, but name is not fit
		priv->ret = 0;

	if (found == 0 && priv->ret == 0)
		priv->ret = UTIL_UBUS_ERROR_ENTRY_NOT_FOUND;
}

int
find_ubus_uci_get(char *config, char *section_type, char *key_value, unsigned int *section_index)
{
	struct blob_buf b;
	unsigned int ubus_path_id;
	int ret, index = -1;
	char section_data[TOKEN_LEN];
	struct util_ubus_uci_get_priv_t priv;

	if (!config || !section_type || !key_value)
		return UTIL_UBUS_ERROR_NULL_POINTER;

	do {
		index++;	//-1 -> 0 (start with 0)
		priv.value_size = strlen(key_value);
		priv.value = key_value;
		priv.ret = UTIL_UBUS_ERROR_ENTRY_NOT_FOUND;

		memset(&b, 0, sizeof(b));	// must! or blob_buf_init would segfault
		blob_buf_init(&b, 0);

		// prepare parameter
		blobmsg_add_string(&b, "config", config);
		snprintf(section_data, TOKEN_LEN, "@%s[%d]", section_type, index);
		blobmsg_add_string(&b, "section", section_data);
		//if(option)
		//      blobmsg_add_string(&b, "option",  option);
		blobmsg_add_string(&b, "ubus_rpc_session", rpc_session_id);

		// check if path 'session' is registered on ubusd
		if ((ret = ubus_lookup_id(ubus_ctx, "uci", &ubus_path_id)) != 0) {
			dbprintf("lookup 'uci' error %d(%s)\n", ret, util_ubus_error_str(ret));
			blob_buf_free(&b);
			return ret;
		}
		//key_value[0] = 0;
		ubus_invoke(ubus_ctx, ubus_path_id, "get", b.head, find_ubus_uci_get_cb, &priv, ubus_invoke_timeout * 1000);
		blob_buf_free(&b);
	} while (priv.ret == 0);

	if (priv.ret < 0) {
		dbprintf("ubus_invoke uci::get cb error %d(%s)\n", priv.ret, util_ubus_error_str(priv.ret));
		return priv.ret;
	}

	if (priv.ret == UTIL_UBUS_OK_ENTRY_FOUND) {
		if (util_ubus_debug)
			dbprintf("found entry %d(%s), index=%d\n", priv.ret, util_ubus_error_str(priv.ret), index);
		*section_index = index;
		return 0;
	}
	return priv.ret;
}

/*
char *section_relation_file		// input configure file
char *child_config,			// input
char *child_section_type,		// input
int child_section_index,		// input
char *parent_config,			// input
char *parent_section_type)		// input
*/

int
find_parent_by_section_type(char *section_relation_file, char *child_config, char *child_section_type, int child_section_index,
			    char *parent_config, char *parent_section_type)
{
	int ret = -1;
	unsigned int parent_section_index;
	char key_name[TOKEN_LEN], key_value[TOKEN_LEN], child_section_data[TOKEN_LEN];

	if (find_key_name_by_parent_child_section
	    (section_relation_file, child_config, child_section_type, parent_config, parent_section_type, key_name) < 0) {
		ret = UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_NAME;
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		return ret;
	}

	snprintf(child_section_data, TOKEN_LEN, "@%s[%d]", child_section_type, child_section_index);
	//use key name get key value
	if ((ret = util_ubus_uci_get(child_config, child_section_data, key_name, key_value, TOKEN_LEN)) < 0) {
		ret = UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_VALUE;
		dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		return ret;
	}
	//parent_config, parent_section_type, key_value
	if (util_ubus_debug)
		dbprintf("key_value=(%s)\n", key_value);

	ret = find_ubus_uci_get(parent_config, parent_section_type, key_value, &parent_section_index);
	if (ret == 0) {
		if (util_ubus_debug) {
			dbprintf("key_value=(%s)\n", key_value);
			dbprintf("found parent section index=(%d)\n", parent_section_index);
		}
		return parent_section_index;
	}
	return ret;
}

/*
char *section_relation_file		// input configure file
char *child_config,			// input
char *child_section_type,		// input
int found_child_section_index[],	// output
int child_section_index_size,		// input
char *parent_config,			// input
char *parent_section_type,		// input
int parent_section_index)		// input
*/

int
find_child_by_section_type(char *section_relation_file, char *child_config, char *child_section_type,
			   int *found_child_section_index, int child_section_index_size, char *parent_config,
			   char *parent_section_type, int parent_section_index)
{
	int ret = -1;
	//unsigned int parent_section_index;
	char key_name[TOKEN_LEN], primary_key_value[TOKEN_LEN], key_value[TOKEN_LEN], parent_section_data[TOKEN_LEN],
	    child_section_data[TOKEN_LEN];
	int index = -1, child_section_index = -1;

	if (find_key_name_by_parent_child_section
	    (section_relation_file, child_config, child_section_type, parent_config, parent_section_type, key_name) < 0) {
		ret = UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_NAME;
		//dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		return ret;
	}

	if (util_ubus_debug >= 2)
		dbprintf("key_name=(%s)\n", key_name);

	//parent_config, parent_section_type, parent_section_index => section_name(key_value)
	snprintf(parent_section_data, TOKEN_LEN, "@%s[%d]", parent_section_type, parent_section_index);
	if ((ret = util_ubus_uci_get(parent_config, parent_section_data, NULL, primary_key_value, TOKEN_LEN)) != 0) {
		ret = UTIL_ERROR_CAN_NOT_FIND_RELATED_KEY_VALUE;
		if (util_ubus_debug >= 2)
			dbprintf("ret=%d(%s)\n\n", ret, util_ubus_error_str(ret));
		return ret;
	}
	if (util_ubus_debug >= 2)
		dbprintf("primary_key_value=(%s)\n", primary_key_value);

	//list for each "child_config, child_section_type"
	//if key_name's value == section_name ==> found one
	do {
		index++;	//-1 -> 0 (start with 0)
		snprintf(child_section_data, TOKEN_LEN, "@%s[%d]", child_section_type, index);
		//use key name get key value
		if ((ret = util_ubus_uci_get(child_config, child_section_data, key_name, key_value, TOKEN_LEN)) == 0) {
			if (util_ubus_debug >= 2)
				dbprintf("get key_value,primary_key_value = (%s),(%s)\n", key_value, primary_key_value);
			//cmpare with primary_key_value
			if (strcmp(primary_key_value, key_value) == 0) {
				child_section_index++;	//-1 -> 0 (start with 0)
				if (util_ubus_debug >= 2)
					dbprintf("found item child_section_index=%d\n\n", child_section_index);
				if (child_section_index < child_section_index_size) {
					*(found_child_section_index + child_section_index) = index;
				} else {
					dbprintf("child_section_index_size=(%d) is too small\n\n", child_section_index_size);
					return UTIL_ERROR_BUFFER_FOR_RETURN_IS_TOO_SMALL;
				}
			}
		}

	} while (ret == 0);

	if (child_section_index == -1)	//can't found any entry 
		return ret;

	//return number of elements in found_child_section_index[]
	return (child_section_index + 1);
}
