/*
   OpenChange Storage Abstraction Layer library
   MAPIStore FSOCPF backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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

#ifndef	__MAPISTORE_FSOCPF_H
#define	__MAPISTORE_FSOCPF_H

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "libocpf/ocpf.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include <dlinklist.h>
#include <dirent.h>

struct fsocpf_folder {
	uint64_t			fid;
	DIR				*dir;
	char				*path;
};

struct fsocpf_folder_list {
	struct fsocpf_folder		*folder;
	struct fsocpf_folder_list	*next;
	struct fsocpf_folder_list	*prev;
};

struct fsocpf_message {
	uint64_t			mid;
	uint64_t			fid;
	uint32_t			ocpf_context_id;
	char				*path;
};

struct fsocpf_message_list {
	struct fsocpf_message		*message;
	struct fsocpf_message_list	*prev;
	struct fsocpf_message_list	*next;
};

struct fsocpf_context {
	void				*private_data;
	char				*uri;
	struct fsocpf_folder_list	*folders;
	struct fsocpf_message_list	*messages;
	uint64_t			fid;
	DIR				*dir;
};

__BEGIN_DECLS

int	mapistore_init_backend(void);

__END_DECLS

#endif /* ! __MAPISTORE_FSOCPF_H */
