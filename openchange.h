#ifndef __OPENCHANGE_H__
#define __OPENCHANGE_H__

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <talloc.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_NOT_DIRENT_H
#include <sys/dir.h>
#else
#include <dirent.h>
#endif

#include <dcerpc.h>

#include <core.h>
#include <core/error.h>
#include <core/nterr.h>
#include <util.h>
#include <param.h>
#include <param/proto.h>
#include <samba/popt.h>
#include <auth.h>
#include <credentials.h>
#include <ndr.h>
#include <popt.h>
#include <ctype.h>

#include <dcerpc_server.h>

#include <gen_ndr/misc.h>
#include <gen_ndr/security.h>
#include <gen_ndr/samr.h>

#include <ldap.h>
#include <ldb.h>
#include <ldb_errors.h>
#include <samdb.h>

#include <db_wrap.h>

#define TEST_USER_NAME		"nspitestuser"
#define	TEST_MACHINE_NAME	"nspitestcomputer"

#endif /* __OPENCHANGE_H__ */
