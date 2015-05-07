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
#ifdef DBG
#undef DBG
#endif
#define DBG(fmt, arg...)    btd_debug("%s:%s() " fmt,  __FILE__, __FUNCTION__ , ## arg)

#define DUN_GW_UUID         "00001103-0000-1000-8000-00805f9b34fb"

int gateway_init(DBusConnection *conn, gboolean secure, const char *modem_bridge_cmd);
void gateway_exit(void);
int gateway_create(struct btd_adapter *adapter, const char *service_name,
                   int rfcomm_channel);
int gateway_destroy(struct btd_adapter *adapter);

