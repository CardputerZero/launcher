/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */


#ifndef _BLUECTL_INTERNAL_H_
#define _BLUECTL_INTERNAL_H_

#include "bluectl.h"
#include <dbus/dbus.h>

#define BLUEZ_NAME             "org.bluez"
#define BLUEZ_ROOT_PATH        "/org/bluez"
#define IFACE_OM               "org.freedesktop.DBus.ObjectManager"
#define IFACE_PROPS            "org.freedesktop.DBus.Properties"
#define IFACE_ADAPTER1         "org.bluez.Adapter1"
#define IFACE_DEVICE1          "org.bluez.Device1"
#define IFACE_AGENT1           "org.bluez.Agent1"
#define IFACE_AGENT_MANAGER1   "org.bluez.AgentManager1"
#define IFACE_MEDIA_PLAYER1    "org.bluez.MediaPlayer1"

#ifdef CONFIG_BLUECTL_AGENT_ENABLED
struct bctl_pending_agent {
	unsigned long id;
	char method[24];
	DBusMessage *msg;
	struct bctl_pending_agent *next;
};
#endif

struct bluectl_ctx {
	DBusConnection *conn;
	int inited;
	unsigned int timeout_ms;
	int err_code;
	char err[192];
	char err_name[96];

	bluectl_event_cb_t ev_cb;
	void *ev_user;

#ifdef CONFIG_BLUECTL_AGENT_ENABLED
	int agent_registered;
	unsigned long agent_next_id;
	struct bctl_pending_agent *pending_agents;
	bluectl_agent_cb_t agent_cb;
	void *agent_user;
#endif
};


extern struct bluectl_ctx g_bluectl;




void bctl_set_err(struct bluectl_ctx *c, int code, const char *fmt, ...);

int bctl_set_dbus_err(struct bluectl_ctx *c, const DBusError *err);

int bctl_check_conn(struct bluectl_ctx *c);


void bctl_strscpy(char *dst, const char *src, size_t len);






DBusMessage *bctl_send(struct bluectl_ctx *c, DBusMessage *call, int timeout_ms);


DBusMessage *bctl_call(struct bluectl_ctx *c, const char *path,
		       const char *iface, const char *method, int timeout_ms);






int bctl_prop_set(struct bluectl_ctx *c, const char *path, const char *iface,
		  const char *name, int type, const void *value);


DBusMessage *bctl_prop_get_all(struct bluectl_ctx *c, const char *path,
			       const char *iface);





int bctl_dict_lookup(DBusMessageIter *dict, const char *name,
		     DBusMessageIter *value);


int bctl_read_bool(DBusMessageIter *it, int def);
long bctl_read_num(DBusMessageIter *it, long def);
void bctl_read_str(DBusMessageIter *it, char *out, size_t len);






typedef void (*bctl_object_cb)(const char *path, const char *iface,
			       DBusMessageIter *props, void *user);
int bctl_foreach_object(struct bluectl_ctx *c, bctl_object_cb cb, void *user);






int bctl_adapter_path(struct bluectl_ctx *c, const char *adapter,
		      char *out, size_t len);
int bctl_device_path(struct bluectl_ctx *c, const char *device,
		     char *out, size_t len);


void bctl_mac_from_path(const char *path, char *out, size_t len);


void bctl_adapter_fill(DBusMessageIter *props, bluectl_adapter_t *out);
void bctl_device_fill(DBusMessageIter *props, bluectl_device_t *out);

#endif /* _BLUECTL_INTERNAL_H_ */
