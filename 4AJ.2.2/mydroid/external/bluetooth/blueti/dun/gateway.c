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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/uuid.h>

#include <glib.h>
#include <gdbus.h>

#include "../src/dbus-common.h"

#include "log.h"
#include "error.h"
#include "sdpd.h"
#include "btio.h"

#include "adapter.h"
#include "gateway.h"
#include "modem_bridge.h"
#include "cutils/properties.h"

#define DUN_GATEWAY_INTERFACE "org.bluez.DunGateway"
#define TTY_TIMEOUT         100
#define TTY_TRIES           10

struct dun_terminal {
	bdaddr_t	dst;		/* Remote Bluetooth Address */
	GIOChannel	*io;		/* RFCOMM client socket */
	guint		watch;		/* RFCOMM socket watch */
	guint		io_watch;
    
	int             rfcomm_ctl;     /* RFCOMM control socket */
	guint           tty_timer;
	int             tty_tries;
	gboolean        tty_open;
	int             tty_id;
	char            tty_name[PATH_MAX];

	GPid            modem_bridge_pid;
};

/* Main gateway structure */
struct dun_gateway {
	struct btd_adapter *adapter;	/* Adapter pointer */
	bdaddr_t	src;		/* Bluetooth Local Address */
	GIOChannel	*io;		/* RFCOMM server socket */
	char		*iface;		/* DBus interface */
	char		*name;		/* Gateway service name */
	char		*bridge;	/* Bridge name */
	uint32_t	record_handle;	/* Service record handle */
	uint16_t	svclass_id;	/* Service class identifier */
	struct dun_terminal *dt;	/* DUN Terminal */
	guint		watch_id;	/* Client service watch */
	uint8_t         channel;        /* RFCOMM DUN GW server channel */
};

static DBusConnection *connection = NULL;
static gboolean bt_high_security = FALSE;
char modem_bridge_cmd_line[PATH_MAX] = "";
static struct dun_gateway *dgw = NULL;

void disconnect_terminal()
{
	struct dun_terminal *dt = dgw->dt;
    
	if (!dt)
		return;
    
	DBG("io %p", dt->io);
    
	if (!dt->io)
		return;

	g_io_channel_shutdown(dt->io, TRUE, NULL);
	g_io_channel_unref(dt->io);
	dt->io = NULL;

	modem_bridge_kill();

	if (dt->tty_timer > 0) {
		g_source_remove(dt->tty_timer);
		dt->tty_timer = 0;
	}

	if (dt->tty_id >= 0) {
		struct rfcomm_dev_req req;

		memset(&req, 0, sizeof(req));
		req.dev_id = dt->tty_id;
		req.flags = (1 << RFCOMM_HANGUP_NOW);
		ioctl(dt->rfcomm_ctl, RFCOMMRELEASEDEV, &req);

		dt->tty_name[0] = '\0';
		dt->tty_open = FALSE;
		dt->tty_id = -1;
	}
    
	dt = 0;
}

static void btio_watchdog_cb(GIOChannel *chan, GIOCondition cond,
				gpointer data)
{
	struct dun_terminal *dt = data;
	char address[18];
	const char *paddr = address;

	DBG("dt %p, condition 0x%x", dt, cond);
    
	if (!connection) return;

	ba2str(&dt->dst, address);
	g_dbus_emit_signal(connection, adapter_get_path(dgw->adapter),
				dgw->iface, "DeviceDisconnected",
				DBUS_TYPE_STRING, &paddr,
				DBUS_TYPE_INVALID);
    
	disconnect_terminal();

	if (dt->io_watch > 0) {
		g_source_remove(dt->io_watch);
		dt->io_watch = 0;
	}
}

static gboolean tty_try_open(gpointer user_data)
{
	struct dun_terminal *dt = user_data;
	int tty_fd;
        char modem_status[PROPERTY_VALUE_MAX];

	DBG("dt %p, tty_name %s", dt, dt->tty_name);
    
	tty_fd = open(dt->tty_name, O_RDONLY | O_NOCTTY);
	if (tty_fd < 0) {
	       DBG("tty_fd %d, errno %d, tty_tries %d", tty_fd, errno, dt->tty_tries);
		if (errno == EACCES)
			goto disconnect;

		dt->tty_tries--;

		if (dt->tty_tries <= 0)
			goto disconnect;

		return TRUE;
	}

	DBG("%s created for DUN", dt->tty_name);

	dt->tty_open = TRUE;
	dt->tty_timer = 0;

	g_io_channel_unref(dt->io);

	dt->io = g_io_channel_unix_new(tty_fd);
	dt->io_watch = g_io_add_watch(dt->io,
					G_IO_HUP | G_IO_ERR | G_IO_NVAL,
					btio_watchdog_cb, dt);

        /* Check if modem is present */
        property_get("modem.ste_rfkill",modem_status,0);

        if(!strcmp(modem_status,"1")) {
           DBG("modem present.Setting modem bridge setup");
           if (!modem_bridge_setup(modem_bridge_cmd_line))
                 goto disconnect;
        }

	return FALSE;

disconnect:
	dt->tty_timer = 0;
       disconnect_terminal();
	return FALSE;
}

static gboolean create_tty(struct dun_terminal *dt)
{
	struct rfcomm_dev_req req;
	int sk = g_io_channel_unix_get_fd(dt->io);
	int sz;

	DBG("dt %p, sk %d", dt, sk);
    
	memset(&req, 0, sizeof(req));
	req.dev_id = -1;
	req.flags = (1 << RFCOMM_REUSE_DLC) | (1 << RFCOMM_RELEASE_ONHUP);

	bacpy(&req.src, &dgw->src);
	bacpy(&req.dst, &dt->dst);

	bt_io_get(dt->io, BT_IO_RFCOMM, NULL,
			BT_IO_OPT_DEST_CHANNEL, &req.channel,
			BT_IO_OPT_INVALID);

	dt->tty_id = ioctl(sk, RFCOMMCREATEDEV, &req);
	if (dt->tty_id < 0) {
		error("Can't create RFCOMM TTY: %s", strerror(errno));
		return FALSE;
	}

	DBG("created tty_id %d", dt->tty_id);
	snprintf(dt->tty_name, PATH_MAX - 1, "/dev/rfcomm%d", dt->tty_id);
	dt->tty_tries = TTY_TRIES;

	tty_try_open(dt);
	if (!dt->tty_open && dt->tty_tries > 0)
		dt->tty_timer = g_timeout_add(TTY_TIMEOUT, tty_try_open, dt);

	return TRUE;
}

static void btio_connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	struct dun_gateway *dg = user_data;
	struct dun_terminal *dt = dg->dt;
	char address[18];
	const char *paddr = address;

	DBG("dt %p", dt);
    
	if (err) {
		error("Establishing DUN connection failed: %s", err->message);
		disconnect_terminal();
		return;
	}

	ba2str(&dt->dst, address);   
	gboolean result = g_dbus_emit_signal(connection, adapter_get_path(dg->adapter),
				dg->iface, "DeviceConnected",
				DBUS_TYPE_STRING, &paddr,
				DBUS_TYPE_INVALID);

	if (!create_tty(dt)) {
		error("RFCOMM TTY device creation failed");
		disconnect_terminal();
	}
}

static void authorization_cb(DBusError *derr, void *user_data)
{
	struct dun_gateway *dg = user_data;
	struct dun_terminal *dt = dg->dt;
	GError *err = NULL;

	DBG("dt %p", dt);
    
	if (derr && dbus_error_is_set(derr)) {
		error("DUN access denied: %s", derr->message);
		goto drop;
	}

	if (!bt_io_accept(dt->io, btio_connect_cb, dg, NULL, &err)) {
		error("bt_io_accept: %s", err->message);
		g_error_free(err);
		goto drop;
	}

	return;

drop:
	disconnect_terminal();
}

static void btio_confirm_cb(GIOChannel *io, gpointer user_data)
{
	struct dun_gateway *dg = user_data;
	struct dun_terminal *dt = dg->dt;
	int perr;
	GError *err = NULL;
	char address[18];

	DBG("dt %p, dt->io %p, io %p", dt, dt->io, io);
    
	if (dt->io) {
		error("Rejecting DUN terminal connection since one already exists");
		return;
	}

	bt_io_get(io, BT_IO_RFCOMM, &err,
			BT_IO_OPT_DEST_BDADDR, &dt->dst,
			BT_IO_OPT_INVALID);
	if (err != NULL) {
		error("Unable to get DUN terminal address: %s", err->message);
		g_error_free(err);
		return;
	}

	perr = btd_request_authorization(&dg->src, &dt->dst, DUN_GW_UUID,
						authorization_cb, user_data);
	if (perr < 0) {
		error("Refusing DUN terminal connection from %s: %s (%d)", dt->dst.b,
				strerror(-perr), -perr);
		return;
	}
    
	ba2str(&dt->dst, address);   
	DBG("DUN GW: incoming connect from terminal %s", address);

	dt->io = g_io_channel_ref(io);
	return;
}

static struct dun_terminal *create_terminal(struct dun_gateway *dg)
{
	struct dun_terminal *dt;
	GIOChannel *io;
       int rfcomm_sock;
	GError *err = NULL;

	DBG("create_terminal(): dg %p, channel %d, security high %d",
        dg, dg->channel, bt_high_security);
    
	io = bt_io_listen(BT_IO_RFCOMM, NULL, btio_confirm_cb, dg, NULL, &err,
				BT_IO_OPT_SOURCE_BDADDR, &dg->src,
				BT_IO_OPT_CHANNEL, dg->channel,
				BT_IO_OPT_SEC_LEVEL,
				bt_high_security ? BT_IO_SEC_HIGH : BT_IO_SEC_MEDIUM,
				BT_IO_OPT_INVALID);
	if (err != NULL) {
		error("Failed to start DUN GW RFCOMM server channel %d, %s",
              dg->channel, err->message);
		g_error_free(err);
		return NULL;
	}

	rfcomm_sock = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_RFCOMM);
	if (rfcomm_sock < 0) {
		error("Unable to create RFCOMM control socket: %s (%d)",
						strerror(errno), errno);
        	if (io != NULL)
        		g_io_channel_unref(io);
        	return NULL;
	}
    
	dt = g_new0(struct dun_terminal, 1);
       dg->dt = dt;
	dg->io = io;    /* Server socket */
	dt->io = 0;     /* Client socket */
       dt->rfcomm_ctl = rfcomm_sock;

	return dt;
}

static void destroy_terminal(struct dun_terminal *dt)
{
       DBG("dt %p", dt);
    
	if (dt->io != NULL) {
		g_io_channel_shutdown(dt->io, TRUE, NULL);
		g_io_channel_unref(dt->io);
	}

	g_free(dt);
}

static void free_gateway(struct dun_gateway*dg)
{
       int err;

	DBG("dg %p", dg);
    
	if (!dg)
		return;

	if (dg->record_handle) {
              err = remove_record_from_server(dg->record_handle);
              if (!err) {
                   dg->record_handle = NULL;
              } else {
		     error("Error removing record from server %s (%d)", strerror(-err), -err);
              }
	}

	g_free(dg->iface);
	g_free(dg->name);
	g_free(dg);
}

static sdp_record_t *create_sdp_record(const char *name, uint8_t ch)
{
	sdp_list_t *svclass_id, *pfseq, *apseq, *root, *aproto;
	uuid_t root_uuid, dun, gn, l2cap, rfcomm;
	sdp_profile_desc_t profile;
	sdp_list_t *proto[2];
	sdp_record_t *record;
	sdp_data_t *channel;

	DBG("name %s, channel %d", name, ch);
    
	record = sdp_record_alloc();
	if (!record)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root);

	sdp_uuid16_create(&dun, DIALUP_NET_SVCLASS_ID);
	svclass_id = sdp_list_append(NULL, &dun);
	sdp_uuid16_create(&gn,  GENERIC_NETWORKING_SVCLASS_ID);
	svclass_id = sdp_list_append(svclass_id, &gn);
	sdp_set_service_classes(record, svclass_id);

	sdp_uuid16_create(&profile.uuid, DIALUP_NET_PROFILE_ID);
	profile.version = 0x0100;
	pfseq = sdp_list_append(NULL, &profile);
	sdp_set_profile_descs(record, pfseq);

	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(NULL, &l2cap);
	apseq = sdp_list_append(NULL, proto[0]);

	sdp_uuid16_create(&rfcomm, RFCOMM_UUID);
	proto[1] = sdp_list_append(NULL, &rfcomm);
	channel = sdp_data_alloc(SDP_UINT8, &ch);
	proto[1] = sdp_list_append(proto[1], channel);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(0, apseq);
	sdp_set_access_protos(record, aproto);

	sdp_set_info_attr(record, name, 0, 0);

	sdp_data_free(channel);
	sdp_list_free(root, NULL);
	sdp_list_free(svclass_id, NULL);
	sdp_list_free(proto[0], NULL);
	sdp_list_free(proto[1], NULL);
	sdp_list_free(pfseq, NULL);
	sdp_list_free(apseq, NULL);
	sdp_list_free(aproto, NULL);

	return record;
}

static uint32_t register_sdp_record(struct dun_gateway *dg)
{
	sdp_record_t *record;

	DBG("register_sdp_record(): dg %p", dg);
    
	record = create_sdp_record(dg->name, dg->channel);
	if (!record) {
		error("Unable to allocate new service record");
		return 0;
	}

	if (add_record_to_server(&dg->src, record) < 0) {
		error("Failed to register service record");
		sdp_record_free(record);
		return 0;
	}

	DBG("Got record handle 0x%x", record->handle);

	return record->handle;
}

static void dbus_disconnect_cb(DBusConnection *conn, void *user_data)
{
	struct dun_gateway *dg = user_data;

	DBG("dbus_disconnect_cb(): dg %p", dg);
    
	dg->watch_id = 0;

	if (dg->record_handle) {
		remove_record_from_server(dg->record_handle);
		dg->record_handle = 0;
	}
    
	disconnect_terminal();
	gateway_destroy(dg->adapter);
}

int gateway_init(DBusConnection *conn, gboolean high_security,
                 const char *modem_bridge_cmd)
{
	DBG("high_security %d, modem bridge cmd line: %s",
        high_security, modem_bridge_cmd);
    
	bt_high_security = high_security;
	connection = dbus_connection_ref(conn);

       if (strlen(modem_bridge_cmd) > PATH_MAX - 1) {
            error("Modem bridge command line is too long!");
            return -1;
       }
       
	strcpy(modem_bridge_cmd_line, modem_bridge_cmd);

	return 0;
}

void gateway_exit(void)
{
	DBG("gateway_exit()");

	if (dgw) {
        	if (dgw->record_handle) {
        		remove_record_from_server(dgw->record_handle);
        		dgw->record_handle = 0;
        	}
            
        	disconnect_terminal();
		gateway_destroy(dgw->adapter);
	}
    
    
	dbus_connection_unref(connection);
	connection = NULL;
}

static DBusMessage *gateway_enable(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct dun_gateway *dg = data;

	DBG("gateway_enable(): dg %p", dg);
    
	if (dg->record_handle)
		return btd_error_already_exists(msg);

	dg->dt = create_terminal(dg);
	if (!dg->dt)
		return btd_error_failed(msg, "DUN GW RFCOMM channel creation failed");
    
	dg->record_handle = register_sdp_record(dg);
	if (!dg->record_handle) {
        	if (dg->io != NULL)
        		g_io_channel_unref(dg->io);
		return btd_error_failed(msg, "DUN GW SDP record registration failed");
	}

	dg->watch_id = g_dbus_add_disconnect_watch(conn,
					dbus_message_get_sender(msg),
					dbus_disconnect_cb, dg, NULL);

	return (dbus_message_new_method_return(msg));
}

static DBusMessage *gateway_disable(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct dun_gateway*dg = data;

	DBG("gateway_disable(): dg %p", dg);
    
	g_dbus_remove_watch(conn, dg->watch_id);

	if (dg->record_handle) {
		remove_record_from_server(dg->record_handle);
		dg->record_handle = 0;
	}
	disconnect_terminal();

	g_io_channel_shutdown(dg->io, TRUE, NULL);
	g_io_channel_unref(dg->io);
    
	return (dbus_message_new_method_return(msg));
}

static DBusMessage *gateway_disconnect_device(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct dun_gateway *dg = data;
	struct dun_terminal *dt = dg->dt;

	DBG("gateway_disconnect_device(): dt %p", dt);
    
	if (dt->io) {
		DBG("Disconnecting DUN terminal device");
		disconnect_terminal();
	} else
		return btd_error_not_connected(msg);

	return (dbus_message_new_method_return(msg));
}

static const GDBusMethodTable gateway_methods[] = {
	{ "Enable", "", "", gateway_enable,  G_DBUS_METHOD_FLAG_ASYNC },
	{ "Disable", "", "",	gateway_disable, G_DBUS_METHOD_FLAG_ASYNC },
	{ "DisconnectDevice", "", "",	gateway_disconnect_device,
                                                                G_DBUS_METHOD_FLAG_ASYNC },
	{ }
};

static GDBusSignalTable gateway_signals[] = {
	{ "DeviceConnected", "s" },
	{ "DeviceDisconnected", "s" },
	{ }
};

static void path_unregister(void *data)
{
	struct dun_gateway *dg = data;
	struct dun_terminal *dt = dg->dt;

	DBG("Unregistered interface %s on path %s",
		dg->iface, adapter_get_path(dg->adapter));

	destroy_terminal(dt);
	free_gateway(dg);

}

int gateway_create(struct btd_adapter *adapter, const char *service_name,
                   int rfcomm_channel)
{
	const char *path;

	DBG("gateway_create(): service_name %s, rfcomm_channel %d", service_name,
        rfcomm_channel);
    
       if (!dgw) {
	    dgw = g_new0(struct dun_gateway, 1);
       }

	dgw->adapter = btd_adapter_ref(adapter);
	dgw->iface = g_strdup(DUN_GATEWAY_INTERFACE);
	dgw->name = g_strdup(service_name);
       dgw->channel = rfcomm_channel;
       dgw->io = 0;

	path = adapter_get_path(adapter);

	if (!g_dbus_register_interface(connection, path, dgw->iface,
					gateway_methods, gateway_signals, NULL,
					dgw, path_unregister)) {
		error("D-Bus failed to register %s interface",
				dgw->iface);
		free_gateway(dgw);
		return -1;
	}

	adapter_get_address(adapter, &dgw->src);
	dgw->svclass_id = DIALUP_NET_SVCLASS_ID;
	dgw->record_handle = 0;

	DBG("Registered interface %s on path %s", dgw->iface, path);

	return 0;
}

int gateway_destroy(struct btd_adapter *adapter)
{
	DBG("gateway_destroy(): adapter %p", adapter);
    
	if (!dgw)
		return -EINVAL;

	g_dbus_unregister_interface(connection, adapter_get_path(adapter),
					dgw->iface);
	btd_adapter_unref(dgw->adapter);
       free_gateway(dgw);

       dgw = NULL;

	return 0;
}

