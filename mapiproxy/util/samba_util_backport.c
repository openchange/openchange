/*
   samba_util backported functions from 4.3.x

   OpenChange Project

   Copyright (C) Jesús García Sáez 2016

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

#include <stdbool.h>
#include <string.h>
#include "mapiproxy/libmapiproxy/libmapiproxy.h"


#if SAMBA_VERSION_MAJOR == 4 && SAMBA_VERSION_MINOR < 2

static size_t strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;

	if (bufsize <= 0) {
		return 0;
	}
	if (len >= bufsize) {
		len = bufsize - 1;
	}
	memcpy(d, s, len);
	d[len] = 0;
	return ret;
}

/* Function from samba_util.c */
char *server_id_str_buf(struct server_id id, struct server_id_buf *dst)
{
	if (server_id_is_disconnected(&id)) {
		strlcpy(dst->buf, "disconnected", sizeof(dst->buf));
	} else if ((id.vnn == NONCLUSTER_VNN) && (id.task_id == 0)) {
		snprintf(dst->buf, sizeof(dst->buf), "%llu",
			 (unsigned long long)id.pid);
	} else if (id.vnn == NONCLUSTER_VNN) {
		snprintf(dst->buf, sizeof(dst->buf), "%llu.%u",
			 (unsigned long long)id.pid, (unsigned)id.task_id);
	} else if (id.task_id == 0) {
		snprintf(dst->buf, sizeof(dst->buf), "%u:%llu",
			 (unsigned)id.vnn, (unsigned long long)id.pid);
	} else {
		snprintf(dst->buf, sizeof(dst->buf), "%u:%llu.%u",
			 (unsigned)id.vnn,
			 (unsigned long long)id.pid,
			 (unsigned)id.task_id);
	}
	return dst->buf;
}

#endif
