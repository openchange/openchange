/*
 * NOTE:  
 *
 * This file imported from the Samba4 project which itself is imported
 * from the Squid project.  The licence below is reproduced intact,
 * but refers to files in Squid's repository, not in Samba nor
 * OpenChange.  See COPYING for the GPLv3 notice (being the later
 * version mentioned below).
 *
 * This file has also been modified, in particular to use talloc to
 * allocate in rfc1738_escape()
 *
 * OpenChange version has been modified to include . as an escapable
 * character for unix and offer the ability to extend the scope for
 * other operating systems in the future.
 *
 * - Andrew Bartlett Oct-2009
 * - Julien Kerihuel <j.kerihuel@openchange.org> March 2011
 */


/*
 * $Id$
 *
 * DEBUG:
 * AUTHOR: Harvest Derived
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include "mapistore_backend.h"
#include "mapistore_common.h"

/*
 *  RFC 1738 defines that these characters should be escaped, as well
 *  any non-US-ASCII character or anything between 0x00 - 0x1F.
 */
static char rfc1738_unsafe_chars[] = {
    (char) 0x3C,		/* < */
    (char) 0x3E,		/* > */
    (char) 0x22,		/* " */
    (char) 0x23,		/* # */
#if 0				/* done in code */
    (char) 0x25,		/* % */
#endif
    (char) 0x7B,		/* { */
    (char) 0x7D,		/* } */
    (char) 0x7C,		/* | */
    (char) 0x5C,		/* \ */
    (char) 0x5E,		/* ^ */
    (char) 0x7E,		/* ~ */
    (char) 0x5B,		/* [ */
    (char) 0x5D,		/* ] */
    (char) 0x60,		/* ` */
    (char) 0x27,		/* ' */
    (char) 0x20			/* space */
};

static char rfc1738_reserved_chars[] = {
    (char) 0x3b,		/* ; */
    (char) 0x2f,		/* / */
    (char) 0x3f,		/* ? */
    (char) 0x3a,		/* : */
    (char) 0x40,		/* @ */
    (char) 0x3d,		/* = */
    (char) 0x26			/* & */
};

/* */
static char mapistore_reserved_unix_chars[] = {
	(char) 0x2e,			/* . */
	(char) 0x2f,			/* / */
};

enum mapistore_encoding {
	MAPISTORE_ENCODING_RFC1738_UNSAFE_CHARS			= (int) -1,
	MAPISTORE_ENCODING_RFC1738_UNSAFE_CHARS_AND_PERCENT	= (int) 0,
	MAPISTORE_ENCODING_RFC1738_RESERVED_CHARS		= (int) 1,
	MAPISTORE_ENCODING_UNIX_RESERVED_CHARS			= (int) 2,
	MAPISTORE_ENCODING_WINDOWS_RESERVED_CHARS		= (int) 3
};

/*
 *  do_escape - Returns a static buffer contains the RFC 1738
 *  compliant or not (depending on encoding value), escaped version of
 *  the given url
 *
 */
static char *
mapistore_util_do_escape(TALLOC_CTX *mem_ctx, const char *url, enum mapistore_encoding encoding)
{
    size_t bufsize = 0;
    const char *p;
    char *buf;
    char *q;
    unsigned int i, do_escape;

    bufsize = strlen(url) * 3 + 1;
    buf = talloc_array(mem_ctx, char, bufsize);
    if (!buf) {
	    return NULL;
    }

    talloc_set_name_const(buf, buf);
    buf[0] = '\0';

    for (p = url, q = buf; *p != '\0' && q < (buf + bufsize - 1); p++, q++) {
        do_escape = 0;

        /* RFC 1738 defines these chars as unsafe */
        for (i = 0; i < sizeof(rfc1738_unsafe_chars); i++) {
            if (*p == rfc1738_unsafe_chars[i]) {
                do_escape = 1;
                break;
            }
        }

        /* Handle % separately */
        if (encoding >= 0 && *p == '%')
            do_escape = 1;

	switch (encoding) {
	case MAPISTORE_ENCODING_RFC1738_RESERVED_CHARS:
		/* RFC 1738 defines these chars as reserved */
		for (i = 0; i < sizeof(rfc1738_reserved_chars); i++) {
			if (*p == rfc1738_reserved_chars[i]) {
				do_escape = 1;
				break;
			}
		}
		break;
	case MAPISTORE_ENCODING_UNIX_RESERVED_CHARS:
		for (i = 0; i < sizeof(mapistore_reserved_unix_chars); i++) {
			if (*p == mapistore_reserved_unix_chars[i]) {
				do_escape = 1;
				break;
			}
		}
		break;
	default:
		break;
	}

        /* RFC 1738 says any control chars (0x00-0x1F) are encoded */
        if ((unsigned char) *p <= (unsigned char) 0x1F) {
            do_escape = 1;
        }
        /* RFC 1738 says 0x7f is encoded */
        if (*p == (char) 0x7F) {
            do_escape = 1;
        }
        /* RFC 1738 says any non-US-ASCII are encoded */
        if (((unsigned char) *p >= (unsigned char) 0x80)) {
            do_escape = 1;
        }
        /* Do the triplet encoding, or just copy the char */
        /* note: while we do not need snprintf here as q is appropriately
         * allocated, Samba does to avoid our macro banning it -- abartlet */

        if (do_escape == 1) {
		(void) snprintf(q, 4, "%%%02X", (unsigned char) *p);
            q += sizeof(char) * 2;
        } else {
            *q = *p;
        }
    }
    *q = '\0';
    return (buf);
}

/*
 * rfc1738_escape - Returns a buffer that contains the RFC
 * 1738 compliant, escaped version of the given url. (escapes unsafe and % characters)
 */
char *
mapistore_util_rfc1738_escape(TALLOC_CTX *mem_ctx, const char *url)
{
	return mapistore_util_do_escape(mem_ctx, url, MAPISTORE_ENCODING_RFC1738_UNSAFE_CHARS_AND_PERCENT);
}

/*
 * rfc1738_escape_unescaped - Returns a buffer that contains
 * the RFC 1738 compliant, escaped version of the given url (escapes unsafe chars only)
 */
char *
mapistore_util_rfc1738_escape_unescaped(TALLOC_CTX *mem_ctx, const char *url)
{
	return mapistore_util_do_escape(mem_ctx, url, MAPISTORE_ENCODING_RFC1738_UNSAFE_CHARS);
}

/*
 * rfc1738_escape_part - Returns a buffer that contains the RFC
 * 1738 compliant, escaped version of the given url segment. (escapes
 * unsafe, reserved and % chars) It would mangle the :// in http://,
 * and mangle paths (because of /).
 */
char *
mapistore_util_rfc1738_escape_part(TALLOC_CTX *mem_ctx, const char *url)
{
	return mapistore_util_do_escape(mem_ctx, url, MAPISTORE_ENCODING_RFC1738_RESERVED_CHARS);
}

char *
mapistore_util_unix_name_escape(TALLOC_CTX *mem_ctx, const char *url)
{
	return mapistore_util_do_escape(mem_ctx, url, MAPISTORE_ENCODING_UNIX_RESERVED_CHARS);
}

/*
 *  rfc1738_unescape() - Converts escaped characters (%xy numbers) in
 *  given the string.  %% is a %. %ab is the 8-bit hexadecimal number "ab"
 */
_PUBLIC_ void
mapistore_util_string_unescape(char *s)
{
    char hexnum[3];
    int i, j;			/* i is write, j is read */
    unsigned int x;
    for (i = j = 0; s[j]; i++, j++) {
        s[i] = s[j];
        if (s[i] != '%')
            continue;
        if (s[j + 1] == '%') {	/* %% case */
            j++;
            continue;
        }
        if (s[j + 1] && s[j + 2]) {
            if (s[j + 1] == '0' && s[j + 2] == '0') {	/* %00 case */
                j += 2;
                continue;
            }
            hexnum[0] = s[j + 1];
            hexnum[1] = s[j + 2];
            hexnum[2] = '\0';
            if (1 == sscanf(hexnum, "%x", &x)) {
                s[i] = (char) (0x0ff & x);
                j += 2;
            }
        }
    }
    s[i] = '\0';
}
