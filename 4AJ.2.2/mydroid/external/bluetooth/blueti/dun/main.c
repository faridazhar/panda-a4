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

#include <errno.h>

#include <gdbus.h>

#include "plugin.h"
#include "log.h"
#include "manager.h"

static DBusConnection *connection;

static int dun_init(void)
{
	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return -EIO;

	if (dun_manager_init(connection) < 0) {
		dbus_connection_unref(connection);
		return -EIO;
	}

	return 0;
}

static void dun_exit(void)
{
	dun_manager_exit();

	dbus_connection_unref(connection);
}

BLUETOOTH_PLUGIN_DEFINE(dun, VERSION,
			BLUETOOTH_PLUGIN_PRIORITY_DEFAULT, dun_init, dun_exit)
