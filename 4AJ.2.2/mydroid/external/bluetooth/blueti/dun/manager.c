/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012 Texas Instruments, Inc.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/uuid.h>

#include <glib.h>
#include <gdbus.h>

#include "log.h"

#include "adapter.h"
#include "device.h"
#include "manager.h"
#include "gateway.h"

#define DUN_SERVICE_NAME_MAX_SIZE 50;

static DBusConnection *connection = NULL;
static char *service_name = NULL;
static char *service_name_default = "Dial-up Networking";
static char *cmd_line = NULL;
static int rfcomm_channel = 22;
static gboolean high_security = FALSE;

static void read_config(const char *file)
{
	GKeyFile *keyfile;
	GError *err = NULL;
	char *str;

	DBG("read_config(): reading file %s", file);
    
	keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(keyfile, file, 0, &err)) {
		g_clear_error(&err);
		goto done;
	}

	rfcomm_channel = g_key_file_get_integer(keyfile, "General",
						"RfcommChannel", &err);
	if (err) {
		DBG("%s: %s", file, err->message);
		g_clear_error(&err);
	} else {
		DBG("RFCOMM channel = %d", rfcomm_channel);
	}

	str = g_key_file_get_string(keyfile, "General", "ServiceName", &err);
	if (err) {
		DBG("%s: %s", file, err->message);
		g_clear_error(&err);
              service_name = service_name_default;
	} else {
		DBG("service name = %s", str);
		service_name = g_strdup(str);
		g_free(str);
	}

	high_security = g_key_file_get_boolean(keyfile, "General",
						"HighSecurity", &err);
	if (err) {
		DBG("%s: %s", file, err->message);
		g_clear_error(&err);
	} else {
		DBG("high security = %d", high_security);
	}

	cmd_line = g_key_file_get_string(keyfile, "General",
						"ModemBridgeCmdLine", &err);
	if (err) {
		DBG("%s: %s", file, err->message);
		g_clear_error(&err);
	} else {
		DBG("command line = %s", cmd_line);
	}

done:
	g_key_file_free(keyfile);
}

static int dun_gw_adapter_probe(struct btd_adapter *adapter)
{
	const gchar *path = adapter_get_path(adapter);

	DBG("path %s", path);

	return gateway_create(adapter, service_name, rfcomm_channel);
}

static void dun_gw_adapter_remove(struct btd_adapter *adapter)
{
	const gchar *path = adapter_get_path(adapter);

	DBG("path %s", path);

	gateway_destroy(adapter);
}

static struct btd_adapter_driver dun_gw_adapter_driver = {
	.name	= "dun-gw-adapter",
	.probe	= dun_gw_adapter_probe,
	.remove	= dun_gw_adapter_remove,
};

int dun_manager_init(DBusConnection *conn)
{
	read_config(CONFIGDIR "/dun.conf");

	if (gateway_init(conn, high_security, cmd_line) < 0)
		return -1;

	/* Register dun gateway if it doesn't exist */
	btd_register_adapter_driver(&dun_gw_adapter_driver);

	connection = dbus_connection_ref(conn);

	return 0;
}

void dun_manager_exit(void)
{
	gateway_exit();

	btd_unregister_adapter_driver(&dun_gw_adapter_driver);

	dbus_connection_unref(connection);
	connection = NULL;
}
