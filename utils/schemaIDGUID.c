/*
   MAPI Implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2006

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

#include <libmapi/libmapi.h>
#include <ldb.h>

static void usage(void)
{
	printf("schemaIDGUID <base64 string>\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	TALLOC_CTX	*mem_ctx;
	DATA_BLOB	blob;
	struct GUID	*guid;
	char		*schemaIDGUID;

	if (argc != 2) {
		usage();
	}

	mem_ctx = talloc_named(NULL, 0, "SchemaIDGUID");

	blob = data_blob_talloc(mem_ctx, argv[1], strlen(argv[1])+1);
	blob.length = ldb_base64_decode((char *)blob.data);
	guid = (struct GUID *) blob.data;
	schemaIDGUID = GUID_string(mem_ctx, guid);
	printf("%s\n", schemaIDGUID);
	return (0);
}
