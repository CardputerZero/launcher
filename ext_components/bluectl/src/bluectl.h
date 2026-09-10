/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */


#ifndef _BLUECTL_H_
#define _BLUECTL_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLUECTL_VERSION "0.1.0"


#define BLUECTL_ADDR_LEN   18	/* "XX:XX:XX:XX:XX:XX" + '\0' */
#define BLUECTL_NAME_LEN   64
#define BLUECTL_PATH_LEN   96
#define BLUECTL_UUID_LEN   37
#define BLUECTL_MAX_UUIDS  12


#define BLUECTL_OK              0
#define BLUECTL_ERR             (-1)
#define BLUECTL_ERR_NO_CONN     (-2)
#define BLUECTL_ERR_NO_ADAPTER  (-3)
#define BLUECTL_ERR_NOT_FOUND   (-4)
#define BLUECTL_ERR_TIMEOUT     (-5)
#define BLUECTL_ERR_INVALID_ARG (-6)
#define BLUECTL_ERR_NO_MEM      (-7)
#define BLUECTL_ERR_NOT_SUPPORT (-8)
#define BLUECTL_ERR_NO_BLUEZ   (-9)








int bluectl_init(void);






void bluectl_deinit(void);


int bluectl_is_connected(void);


const char *bluectl_last_error(void);

const char *bluectl_version(void);






void bluectl_set_timeout(unsigned int timeout_ms);

#define BLUECTL_DEFAULT_TIMEOUT_MS 15000
#define BLUECTL_CONNECT_TIMEOUT_MS 30000
#define BLUECTL_PAIR_TIMEOUT_MS    60000






int bluectl_process(unsigned int timeout_ms);



typedef enum {
	BLUECTL_EV_ADAPTER_ADDED = 1,
	BLUECTL_EV_ADAPTER_REMOVED,
	BLUECTL_EV_DEVICE_ADDED,
	BLUECTL_EV_DEVICE_REMOVED,
	BLUECTL_EV_INTERFACES_ADDED,
	BLUECTL_EV_INTERFACES_REMOVED,
	BLUECTL_EV_PROPERTY_CHANGED,
	BLUECTL_EV_BLUEZ_UP,
	BLUECTL_EV_BLUEZ_DOWN,
} bluectl_event_type_t;





typedef struct {
	bluectl_event_type_t type;
	const char *path;
	const char *interface;
	const char *property;
	const char *value;
} bluectl_event_t;

typedef void (*bluectl_event_cb_t)(const bluectl_event_t *ev, void *user_data);






int bluectl_set_event_callback(bluectl_event_cb_t cb, void *user_data);



typedef struct {
	char path[BLUECTL_PATH_LEN];	/* "/org/bluez/hci0" */
	char address[BLUECTL_ADDR_LEN];
	char name[BLUECTL_NAME_LEN];
	char alias[BLUECTL_NAME_LEN];
	int powered;		/* Powered */
	int discoverable;	/* Discoverable */
	int discovering;
	int pairable;		/* Pairable */
	unsigned int discoverable_timeout;
	unsigned int pairable_timeout;
} bluectl_adapter_t;









int bluectl_get_adapters(bluectl_adapter_t *out, int max);


int bluectl_get_adapter(const char *adapter, bluectl_adapter_t *out);


int bluectl_default_adapter(char *path, size_t len);


int bluectl_set_power(const char *adapter, int on);

int bluectl_set_pairable(const char *adapter, int on);
int bluectl_set_discoverable(const char *adapter, int on);


int bluectl_set_discoverable_timeout(const char *adapter, unsigned int seconds);

int bluectl_set_pairable_timeout(const char *adapter, unsigned int seconds);


int bluectl_set_alias(const char *adapter, const char *alias);


int bluectl_start_discovery(const char *adapter);
int bluectl_stop_discovery(const char *adapter);





int bluectl_remove_device(const char *adapter, const char *device);



typedef struct {
	char path[BLUECTL_PATH_LEN];	/* "/org/bluez/hci0/dev_XX_XX_..." */
	char address[BLUECTL_ADDR_LEN];
	char name[BLUECTL_NAME_LEN];
	char icon[32];
	int connected;		/* Connected */
	int paired;		/* Paired */
	int trusted;		/* Trusted */
	int blocked;		/* Blocked */
	int rssi;
	unsigned int dev_class;	/* Class */
	char uuids[BLUECTL_MAX_UUIDS][BLUECTL_UUID_LEN];
	int uuid_count;
} bluectl_device_t;












int bluectl_get_devices(const char *adapter, bluectl_device_t *out, int max);


int bluectl_get_device(const char *device, bluectl_device_t *out);





int bluectl_pair(const char *device);


int bluectl_connect(const char *device);
int bluectl_disconnect(const char *device);


int bluectl_set_trusted(const char *device, int on);
int bluectl_set_blocked(const char *device, int on);


int bluectl_set_device_alias(const char *device, const char *alias);



#ifdef CONFIG_BLUECTL_AGENT_ENABLED

#define BLUECTL_AGENT_PATH "/org/bluectl/agent"


#define BLUECTL_CAP_DISPLAY_ONLY      "DisplayOnly"
#define BLUECTL_CAP_DISPLAY_YES_NO    "DisplayYesNo"
#define BLUECTL_CAP_KEYBOARD_ONLY     "KeyboardOnly"
#define BLUECTL_CAP_NO_INPUT_NO_OUTPUT "NoInputNoOutput"
#define BLUECTL_CAP_KEYBOARD_DISPLAY  "KeyboardDisplay"







typedef struct {
	unsigned long id;
	const char *method;
	char device[BLUECTL_ADDR_LEN];
	char path[BLUECTL_PATH_LEN];
	char passkey[8];
	char uuid[BLUECTL_UUID_LEN];
	int needs_reply;
} bluectl_agent_request_t;

typedef void (*bluectl_agent_cb_t)(const bluectl_agent_request_t *req, void *user_data);





int bluectl_agent_set_callback(bluectl_agent_cb_t cb, void *user_data);






int bluectl_agent_register(const char *capability);


int bluectl_agent_request_default(void);


int bluectl_agent_unregister(void);







int bluectl_agent_reply(unsigned long id, int accept, const char *code);


int bluectl_agent_cancel_pending(void);

#endif /* CONFIG_BLUECTL_AGENT_ENABLED */



#ifdef CONFIG_BLUECTL_MEDIA_ENABLED

typedef struct {
	char path[BLUECTL_PATH_LEN];
	char device[BLUECTL_ADDR_LEN];
	char status[16];	/* playing/paused/stopped/... */
	char title[BLUECTL_NAME_LEN];
	char artist[BLUECTL_NAME_LEN];
	char album[BLUECTL_NAME_LEN];
	char genre[32];
	unsigned int duration;
	unsigned int position;
} bluectl_media_player_t;





int bluectl_media_player_get(const char *device, bluectl_media_player_t *out);





int bluectl_media_cmd(const char *device, const char *cmd);

#endif /* CONFIG_BLUECTL_MEDIA_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* _BLUECTL_H_ */
