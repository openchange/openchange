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

#include <libmapi/libmapi.h>
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>


struct test_folders {
	const char	*name;
	const char	*class;
};


static struct test_folders subfolders[] = {
	{ MT_DIRNAME_APPOINTMENT,	IPF_APPOINTMENT	},
	{ MT_DIRNAME_CONTACT,		IPF_CONTACT	},
	{ MT_DIRNAME_JOURNAL,		IPF_JOURNAL	},
	{ MT_DIRNAME_POST,		IPF_POST	},
	{ MT_DIRNAME_NOTE,		IPF_NOTE	},
	{ MT_DIRNAME_STICKYNOTE,	IPF_STICKYNOTE	},
	{ MT_DIRNAME_TASK,		IPF_TASK	},
	{ NULL,				NULL		}
};


static void mapitest_call_GetHierarchyTable(struct mapitest *mt,
					    mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_htable;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(obj_folder, &obj_htable);
	mapitest_print_call(mt, "GetHierarchyTable", GetLastError());
	mapi_object_release(&obj_htable);
}


static void mapitest_call_GetContentsTable(struct mapitest *mt,
					   mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_ctable;

	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable);
	mapitest_print_call(mt, "GetContentsTable", GetLastError());
	mapi_object_release(&obj_ctable);
}


static void mapitest_call_CreateFolder(struct mapitest *mt, 
				       mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	struct SPropValue	lpProp[1];
	mapi_object_t		obj_top;
	mapi_object_t		obj_child;
	uint32_t		i;
	char			*tmp;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "CreateFolder");
	mapitest_indent();

	/* Create the top test folder */
	mapi_object_init(&obj_top);
	retval = CreateFolder(obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top);
	

	/* Create Sub directories */
	for (i = 0; subfolders[i].name; i++) {
		mapi_object_init(&obj_child);
		retval = CreateFolder(&obj_top, FOLDER_GENERIC, 
				      subfolders[i].name, NULL,
				      OPEN_IF_EXISTS, &obj_child);
		tmp = talloc_asprintf(mt->mem_ctx, 
				      "Step %d.%d: %-22s created ", i + 1, 0, 
				      subfolders[i].name);
		mapitest_print_subcall(mt, tmp, GetLastError());
		talloc_free(tmp);
		
		/* set its container class */
		set_SPropValue_proptag(&lpProp[0], PR_CONTAINER_CLASS, (const void *) subfolders[i].class);
		retval = SetProps(&obj_child, lpProp, 1);
		tmp = talloc_asprintf(mt->mem_ctx, 
				      "Step %d.%d: %-26s set", i + 1, 1, 
				      subfolders[i].class);
		mapitest_print_subcall(mt, tmp, GetLastError());
		talloc_free(tmp);

		mapi_object_release(&obj_child);
	}
	mapitest_deindent();

	retval = EmptyFolder(&obj_top);
	mapitest_print_call(mt, "EmptyFolder", GetLastError());

	retval = DeleteFolder(obj_folder, mapi_object_get_id(&obj_top));
	mapitest_print_call(mt, "DeleteFolder", GetLastError());

	mapi_object_release(&obj_top);
}


static void mapitest_call_CreateMessage(struct mapitest *mt,
					mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_message;
	mapi_id_t		id_msgs[1];

	mapi_object_init(&obj_message);
	retval = CreateMessage(obj_folder, &obj_message);
	mapitest_print_call(mt, "CreateMessage", GetLastError());

	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(obj_folder, id_msgs, 1);
	mapitest_print_call(mt, "DeleteMessage", GetLastError());

	mapi_object_release(&obj_message);
}


void mapitest_calls_ring3(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_id_t		id_folder;
	uint32_t		attempt = 0;

	mapitest_print_interface_start(mt, "Third Ring");

retry:
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
		attempt++;
		goto retry;
	}

	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapitest_call_GetHierarchyTable(mt, &obj_folder);
	mapitest_call_GetContentsTable(mt, &obj_folder);
	mapitest_call_CreateFolder(mt, &obj_folder);

	mapi_object_release(&obj_folder);
	

	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);

	mapitest_call_CreateMessage(mt, &obj_folder);
	

	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	mapitest_print_interface_end(mt);
}
