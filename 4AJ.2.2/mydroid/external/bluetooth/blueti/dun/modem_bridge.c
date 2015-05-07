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

#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/wait.h>

#include "log.h"
#ifdef DBG
#undef DBG
#endif
#define DBG(fmt, arg...) btd_debug("%s:%s() " fmt,  __FILE__, __FUNCTION__ , ## arg)

#define MODEM_BRIDGE "/system/bin/bridge_relay"


gboolean modem_bridge_setup(const char *cmd_line)
{
       int ret;

	DBG("Starting Modem Bridge with command \"%s\"", cmd_line);

       ret = system (cmd_line);

       if (WIFEXITED(ret))
            DBG("modem_bridge exited with status %d", WEXITSTATUS(ret));
       else
            DBG("modem_bridge was killed by signal %d", WTERMSIG(ret));

	return TRUE;
}

void modem_bridge_kill()
{
    /* Currently 'bridge_relay' utility terminates itself when one of the
     * connected interfaces is disconnected.
     */
}


