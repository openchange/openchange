/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#ifndef __MAPITEST_H__
#define	__MAPITEST_H__

#include <errno.h>
#include <err.h>


/**
 * Data structures
 */

struct mapitest {
	TALLOC_CTX		*mem_ctx;
	struct emsmdb_info	info;
	bool			confidential;
	bool			mapi_all;
	bool			mapi_calls;
	const char		*org;
	const char		*org_unit;
	FILE			*stream;
};


/**
 * Prototypes and definitions
 */

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* Definitions from mapitest_print.c */
void	mapitest_print_headers_start(struct mapitest *);
void	mapitest_print_headers_end(struct mapitest *);
void	mapitest_print_line(struct mapitest *, unsigned int);
void	mapitest_print(struct mapitest *, const char *, ...);
void	mapitest_print_bool(struct mapitest *, const char *, const char *, bool);
void	mapitest_print_headers_info(struct mapitest *);
void	mapitest_print_server_info(struct mapitest *);
void	mapitest_print_options(struct mapitest *);
void	mapitest_print_call(struct mapitest *, const char *, enum MAPISTATUS);
void	mapitest_print_subcall(struct mapitest *, const char *, enum MAPISTATUS);
void	mapitest_print_interface_start(struct mapitest *, const char *);
void	mapitest_print_interface_end(struct mapitest *);

void	mapitest_indent(void);
void	mapitest_deindent(void);

/* Definitions from mapitest_common.c */
bool	mapitest_find_message_subject(struct mapitest *, mapi_object_t *,
				      mapi_object_t *, uint8_t, const char *, 
				      const char *, uint32_t *);

/* Definitions required by mapitest_calls.c */
void	mapitest_calls(struct mapitest *);
void	mapitest_calls_ring1(struct mapitest *);
void	mapitest_calls_ring2(struct mapitest *);
void	mapitest_calls_ring3(struct mapitest *);
void	mapitest_calls_ring4(struct mapitest *);

__END_DECLS


/**
 * Defines
 */

#define	MT_YES		"[yes]"
#define	MT_NO		"[no]"
#define	MT_ERROR       	"[ERROR]: %s\n"
#define	MT_ERRNO       	"[*] %-45s: [ERROR]: 0x%.8x\n"
#define	MT_CALLOK	"[CALL]: %s OK\n"
#define	MT_WARN		"[WARNING]: %s\n"

#define	MT_CONFIDENTIAL	"[Confidential]"

#define	MT_HDR_START		"#############################[mapitest report]#################################\n"
#define	MT_HDR_END		"###############################################################################\n"
#define	MT_HDR_FMT		"[*] %-45s: %-20s\n"
#define	MT_HDR_FMT_DATE		"[*] %-45s: %-20s"
#define	MT_HDR_FMT_SECTION	"[*] %-45s:\n"
#define	MT_HDR_FMT_SUBSECTION	"%-41s: %-10s\n"
#define	MT_HDR_FMT_STORE_VER	"%-41s: %d.%d.%d\n"
#define	MT_INTERFACE_FMT	"[0x%.2x] %s\n"
#define	MT_CALL_FMT_OK		"[*] %-45s: [PASSED]\n"
#define	MT_CALL_FMT_KO		"[*] %-45s: [FAILED]: 0x%.8x\n"
#define	MT_SUBCALL_FMT_OK	"%-41s: [PASSED]\n"
#define	MT_SUBCALL_FMT_KO	"%-41s: [FAILED]: 0x%.8x\n"


#define	MT_DIRNAME_TOP		"[MP] Top of Mailbox"
#define	MT_DIRNAME_APPOINTMENT	"[MP] Calendar"
#define	MT_DIRNAME_CONTACT	"[MP] Contact"
#define	MT_DIRNAME_JOURNAL	"[MP] Journal"
#define	MT_DIRNAME_POST		"[MP] Post"
#define	MT_DIRNAME_NOTE		"[MP] Note"
#define	MT_DIRNAME_STICKYNOTE	"[MP] Sticky Notes"
#define	MT_DIRNAME_TASK		"[MP] Tasks"

#define	MT_MAIL_SUBJECT			"[MP] Mail"
#define	MT_MAIL_SUBJECT_ATTACH		"[MP] Mail - Attach"
#define	MT_MAIL_SUBJECT_READFLAGS	"[MP] Mail - ReadFlags"

/**
 * Macros
 */

#define	MT_ERRNO_IF(mt, x, s) {						\
		if (x != MAPI_E_SUCCESS) {				\
			mapitest_print(mt, MT_ERRNO, s, GetLastError());\
			errno = 0;					\
			return;						\
		}							\
	}

#define	MT_ERRNO_IF_CALL(mt, x, c, s) {						\
		if (x != MAPI_E_SUCCESS) {					\
			char	*str;						\
										\
			str = talloc_asprintf(mt->mem_ctx, "%s:%s", c, s);	\
			mapitest_print(mt, MT_ERRNO, str, GetLastError());	\
			talloc_free(str);					\
			errno = 0;						\
			return;							\
		}								\
	}


#define	MT_ERRNO_IF_CALL_BOOL(mt, x, c, s) {   					\
		if (x != MAPI_E_SUCCESS) {					\
			char	*str;						\
										\
			str = talloc_asprintf(mt->mem_ctx, "%s:%s", c, s);	\
			mapitest_print(mt, MT_ERRNO, str, GetLastError());	\
			talloc_free(str);					\
			errno = 0;						\
			return false;							\
		}								\
	}

#endif /* ! __MAPITEST_H__ */
