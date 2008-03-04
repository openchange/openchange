/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008.

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

#ifndef __OCPF_API_H_
#define	__OCPF_API_H_

#include <libmapi/libmapi.h>

struct ocpf_var
{
	struct ocpf_var		*prev;
	struct ocpf_var		*next;
	const char		*name;
	const void		*value;
};

struct ocpf_oleguid
{
	struct ocpf_oleguid	*prev;
	struct ocpf_oleguid	*next;
	const char		*name;
	const char		*guid;
};

struct ocpf_property
{
	struct ocpf_property	*prev;
	struct ocpf_property	*next;
	uint32_t		aulPropTag;
	const void		*value;
};

struct ocpf_nprop
{
	const char	*OOM;
	const char	*mnid_string;
	uint16_t	mnid_id;
	uint16_t       	propType;
	const char	*guid;
	bool		registered;
};

enum ocpf_ntype {
	OCPF_OOM = 0x1,
	OCPF_MNID_ID,
	OCPF_MNID_STRING
};

struct ocpf_nproperty
{
	struct ocpf_nproperty	*prev;
	struct ocpf_nproperty	*next;
	enum ocpf_ntype		kind;
	const char		*OOM;
	const char		*mnid_string;
	uint16_t		mnid_id;
	uint16_t		propType;
	const char		*oleguid;
	const void		*value;
};

struct ocpf
{
	TALLOC_CTX		*mem_ctx;
	const char		*type;
	struct ocpf_var		*vars;
	struct ocpf_oleguid    	*oleguid;
	struct ocpf_property	*props;
	struct ocpf_nproperty	*nprops;
	const char		*filename;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	uint64_t		folder;
};


struct ocpf_olfolder
{
	int			id;
	const char		*name;
};

#include <libocpf/proto_private.h>

/**
 * Defines
 */
#define	OCPF_WARN(x) (ocpf_do_debug x)				

#define	OCPF_RETVAL_IF(x, msg, mem_ctx)  		       	\
do {								\
	if (x) {						\
		OCPF_WARN(("%s", msg));				\
		if (mem_ctx) {					\
			talloc_free(mem_ctx);			\
		}						\
		return OCPF_ERROR;     				\
	}							\
} while (0);

#define	OCPF_INITIALIZED		"OCPF context has already been initialized"
#define	OCPF_NOT_INITIALIZED		"OCPF context has not been initialized"

#define	OCPF_FATAL_ERROR		"Fatal error encountered"
#define	OCPF_WARN_FILENAME_INVALID	"Invalid filename"

#define	OCPF_WARN_PROP_REGISTERED	"Property already registered"
#define	OCPF_WARN_PROP_TYPE		"Property type not supported"
#define	OCPF_WARN_PROP_UNKNOWN		"Property Unknown"

#define	OCPF_WARN_OOM_UNKNOWN		"Unknown OOM"
#define	OCPF_WARN_OOM_REGISTERED	"OOM already registered"

#define	OCPF_WARN_LID_UNKNOWN		"Unknown MNID_ID"
#define	OCPF_WARN_LID_REGISTEERED	"MNID_ID already registered"

#define	OCPF_WARN_STRING_UNKNOWN	"Unknown MNID_STRING"
#define	OCPF_WARN_STRING_REGISTERED	"MNID_STRING already registered"


#define	OCPF_WARN_OLEGUID_N_REGISTERED	"OLEGUID name already registered"
#define	OCPF_WARN_OLEGUID_G_REGISTERED	"OLEGUID GUID already registered"
#define	OCPF_WARN_OLEGUID_UNREGISTERED	"OLEGUID unregistered"
#define	OCPF_WARN_OLEGUID_INVALID	"OLEGUID invalid"

#define	OCPF_WARN_VAR_REGISTERED	"Variable already registered"
#define	OCPF_WARN_VAR_NOT_REGISTERED	"Unknown variable"
#define	OCPF_WARN_VAR_TYPE		"Variable property type not supported"

#define	OCPF_WARN_FOLDER_ID_UNKNOWN	"Unknown Folder"

#define	DATE_FORMAT     "%Y-%m-%d %H:%M:%S"

#endif /* __OCPF_API_H_ */
