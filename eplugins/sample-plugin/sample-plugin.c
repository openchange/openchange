/*
 *  OpenChange Evolution plugin implementation.
 *
 *  Copyright (C) Julien Kerihuel 2006
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef HAV_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkselection.h>

#include <e-util/e-plugin.h>

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	fprintf(stderr, "OpenChange Sample Evolution Plugin\n");
	return 0;
}
