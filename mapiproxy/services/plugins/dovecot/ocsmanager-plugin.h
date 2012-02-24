/*
   OpenChange Service Manager Plugin for Dovecot

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OCSMANAGER_PLUGIN_H
#define	__OCSMANAGER_PLUGIN_H

#include "lib.h"
#include "llist.h"
#include "array.h"
#include "config.h"
#include "module-dir.h"
#include "mail-user.h"
#include "mail-storage-private.h"
#include "mailbox-list-private.h"
#include "notify-plugin.h"

#include <ctype.h>
#include <stdlib.h>

extern const char *ocsmanager_plugin_dependencies[];

const char *ocsmanager_plugin_version = DOVECOT_VERSION;

void ocsmanager_plugin_init(struct module *);
void ocsmanager_plugin_deinit(void);

#endif /* __OCSMANAGER_PLUGIN_H */
