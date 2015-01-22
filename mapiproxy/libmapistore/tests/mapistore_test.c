/*
   OpenChange Storage Abstraction Layer library test tool

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2014

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

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include <talloc.h>
#include <core/ntstatus.h>
#include <popt.h>
#include <param.h>
#include <util/debug.h>
#include <samba/session.h>

/**
   \file mapistore_test.c

   \brief Test mapistore implementation
 */


extern struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, int);

static struct ldb_context *sam_ldb_init(void)
{
	TALLOC_CTX		*mem_ctx;
	struct loadparm_context *lp_ctx;
	struct ldb_context	*samdb_ctx = NULL;
	struct tevent_context	*ev;
	int			ret;
	struct ldb_result	*res;
	struct ldb_dn		*tmp_dn = NULL;
	static const char	*attrs[] = {
		"rootDomainNamingContext",
		"defaultNamingContext",
		NULL
	};

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	if (!mem_ctx) return NULL;

	ev = tevent_context_init(talloc_autofree_context());
	if (!ev) goto end;

	/* Step 2. Connect to the database */
	lp_ctx = loadparm_init_global(true);
	if (!lp_ctx) goto end;

	samdb_ctx = samdb_connect(NULL, NULL, lp_ctx, system_session(lp_ctx), 0);
	if (!samdb_ctx) goto end;

	/* Step 3. Search for rootDSE record */
	ret = ldb_search(samdb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, samdb_ctx, "@ROOTDSE"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) goto end;
	if (res->count != 1) goto end;

	/* Step 4. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(samdb_ctx, samdb_ctx,
					 res->msgs[0], "rootDomainNamingContext");
	ldb_set_opaque(samdb_ctx, "rootDomainNamingContext", tmp_dn);

	tmp_dn = ldb_msg_find_attr_as_dn(samdb_ctx, samdb_ctx,
					 res->msgs[0], "defaultNamingContext");
	ldb_set_opaque(samdb_ctx, "defaultNamingContext", tmp_dn);

end:
	talloc_free(mem_ctx);

	return samdb_ctx;
}


static uint64_t _find_first_child_folder(TALLOC_CTX *mem_ctx, struct mapistore_context *mstore_ctx,
					 uint32_t context_id, void *folder_object)
{
	uint32_t			i;
	uint32_t			count = 0;
	uint64_t			child_fmid = 0;
	enum mapistore_error		retval;
	void				*table_object;
	struct SPropTagArray		*SPropTagArray;
	struct mapistore_property_data	*row_data;

	retval = mapistore_folder_open_table(mstore_ctx, context_id, folder_object,
					     mem_ctx, MAPISTORE_FOLDER_TABLE, 0,
					     &table_object, &count);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_open_table: %s\n", mapistore_errstr(retval)));
		return 0;
	}
	DEBUG(0, ("open_table: count = %d\n", count));

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_COMMENT_UNICODE,
					  PR_CONTENT_UNREAD,
					  PR_CONTENT_COUNT,
					  PR_FOLDER_CHILD_COUNT);
	retval = mapistore_table_set_columns(mstore_ctx, context_id, table_object,
					     (uint16_t)SPropTagArray->cValues,
					     SPropTagArray->aulPropTag);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_table_set_columns: %s\n", mapistore_errstr(retval)));
		return 0;
	}

	retval = mapistore_table_get_row(mstore_ctx, context_id, table_object,
					 mem_ctx, MAPISTORE_PREFILTERED_QUERY, 0,
					 &row_data);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_table_get_row: %s\n", mapistore_errstr(retval)));
		return 0;
	}

	for (i = 0; i < SPropTagArray->cValues; i++) {
		if (row_data[i].error != MAPISTORE_SUCCESS) {
			DEBUG(0, ("0x%x: MAPI_E_NOT_FOUND\n", SPropTagArray->aulPropTag[i]));
			continue;
		}

		struct SPropValue prop_value;

		prop_value.ulPropTag = SPropTagArray->aulPropTag[i];
		prop_value.dwAlignPad = 0;
		set_SPropValue(&prop_value, row_data[i].data);
		mapidump_SPropValue(prop_value, NULL);

		if (prop_value.ulPropTag == PR_FID) {
			child_fmid = prop_value.value.d;
		}
	}

	talloc_free(row_data);

	return child_fmid;
}


static enum mapistore_error _test_folder_create_delete(TALLOC_CTX *mem_ctx,
						       struct mapistore_context *mstore_ctx,
						      uint32_t context_id, void *parent_folder)
{
	struct SRow 		*aRow;
	void			*child_folder;
	void			*child_subfold;
	enum mapistore_error	retval;

	aRow = talloc_array(mem_ctx, struct SRow, 1);
	aRow->lpProps = talloc_array(aRow, struct SPropValue, 1);
	aRow->cValues = 0;
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PidTagDisplayName, (void *)("FolderToCreate"));
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PidTagComment, (void *)("This is a good comment"));

	retval = mapistore_folder_create_folder(mstore_ctx, context_id, parent_folder, NULL,
						0x123456, aRow, &child_folder);
	TALLOC_FREE(aRow);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_create_folder: %s\n", mapistore_errstr(retval)));
		return retval;
	}


	/* folder creation */
	aRow = talloc_array(mem_ctx, struct SRow, 1);
	aRow->lpProps = talloc_array(aRow, struct SPropValue, 1);
	aRow->cValues = 0;
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PidTagDisplayName, (void *)("FolderToCreate 2"));
	aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
				       PidTagComment, (void *)("This is a good comment 2"));

	retval = mapistore_folder_create_folder(mstore_ctx, context_id, child_folder, NULL,
						0x456789, aRow, &child_subfold);
	TALLOC_FREE(aRow);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_create_folder: %s\n", mapistore_errstr(retval)));
		return retval;
	}

	/* delete folder */
	retval = mapistore_folder_delete(mstore_ctx, context_id, child_subfold, 0);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_delete: %s\n", mapistore_errstr(retval)));
		return retval;
	}

	retval = mapistore_folder_delete(mstore_ctx, context_id, child_folder, 0);
	if (retval != MAPISTORE_ERR_NOT_FOUND) {
		DEBUG(0, ("mapistore_folder_delete: %s\n", mapistore_errstr(retval)));
		return retval;
	}

	return MAPISTORE_SUCCESS;
}


int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	int				retval;
	struct mapistore_context	*mstore_ctx;
	struct mapistore_contexts_list	*mstore_contexts;
	struct loadparm_context		*lp_ctx;
	void				*openchangedb_ctx;
	struct ldb_context		*samdb_ctx;
	poptContext			pc;
	int				opt;
	const char			*opt_debug = NULL;
	const char			*opt_uri = NULL;
	const char			*opt_username = NULL;
	uint64_t			fmid;
	uint64_t			root_fmid;
	uint32_t			context_id = 0;
	void				*folder_object;
	void				*child_folder_object;
	void				*table_object;
	void				*message_object;
	uint32_t			i;
	uint32_t			j;
	struct SPropTagArray		*SPropTagArray;


	enum { OPT_DEBUG=1000, OPT_USERNAME, OPT_URI };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,	"set the debug level", NULL },
		{ "username", 'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "set mapistore backend user", NULL },
		{ "uri", 'U', POPT_ARG_STRING, NULL, OPT_URI, "set the backend URI", NULL},
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "mapistore_test");
	lp_ctx = loadparm_init_global(false);
	setup_logging(NULL, DEBUG_STDOUT);
	
	pc = poptGetContext("mapistore_test", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_USERNAME:
			opt_username = poptGetOptArg(pc);
			break;
		case OPT_URI:
			opt_uri = poptGetOptArg(pc);
			break;
		}
	}

	poptFreeContext(pc);

	if (!opt_uri) {
		DEBUG(0, ("Usage: mapistore_test -U <opt_uri> [OPTION]\n"));
		exit(0);
	}

	if (opt_debug) {
		lpcfg_set_cmdline(lp_ctx, "log level", opt_debug);
	}
	
	if (!opt_username) {
		opt_username = getenv("USER");
		DEBUG(0, ("[WARN]: No username specified - default to %s\n", opt_username));
	}

	if (!lpcfg_load_default(lp_ctx)) {
		DEBUG(0, ("[WARN]: lpcfg_load_default failed\n"));
	}

	retval = mapistore_set_mapping_path("/tmp");
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[ERR]: %s\n", mapistore_errstr(retval)));
		exit (1);
	}

	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (!mstore_ctx) {
		DEBUG(0, ("[ERR]: Unable to initialize mapistore context\n"));
		exit (1);
	}

	openchangedb_ctx = mapiproxy_server_openchangedb_init(lp_ctx);
	if (!openchangedb_ctx) {
		DEBUG(0, ("[ERR]: Unable to initialize openchange database\n"));
		exit (1);
	}

	samdb_ctx = sam_ldb_init();
	if (!samdb_ctx) {
		DEBUG(0, ("[ERR]: Unable to open SAM database\n"));
		exit (1);
	}

	retval = mapistore_set_connection_info(mstore_ctx, samdb_ctx, openchangedb_ctx, opt_username);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[ERR]: %s\n", mapistore_errstr(retval)));
		exit (1);
	}

	DEBUG(0, ("*** mapistore_backend_get_path\n"));
	retval = mapistore_list_contexts_for_user(mstore_ctx, opt_username, mem_ctx, &mstore_contexts);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[ERR]: mapistore_list_contexts_for_user: %s\n", mapistore_errstr(retval)));
		// TODO: Fail here when we start to support list_contexts
	}
	for (; mstore_contexts; mstore_contexts = mstore_contexts->next) {
		DEBUG(0, ("context %s\n", mstore_contexts->url));
		DEBUG(0, ("\t.name %s\n", mstore_contexts->name));
		DEBUG(0, ("\t.main_folder %d\n", mstore_contexts->main_folder));
		DEBUG(0, ("\t.role %d\n", mstore_contexts->role));
		DEBUG(0, ("\t.tag %s\n", mstore_contexts->tag));
	}
	DEBUG(0, ("*** mapistore_add_context\n"));
	{
		bool soft_deleted;
		retval = mapistore_indexing_record_get_fmid(mstore_ctx, opt_username, opt_uri, false,
							    &root_fmid, &soft_deleted);
		if (retval != MAPISTORE_SUCCESS || !root_fmid) {
			root_fmid = 0xdeadbeef00000001L;
		}
	}
	retval = mapistore_add_context(mstore_ctx, opt_username, opt_uri, root_fmid, &context_id, &folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[ERR]: %s\n", mapistore_errstr(retval)));
		goto end;
	}

	/* get_path */
	{
		char			*mapistore_URI = NULL;
		struct backend_context	*backend_ctx;

		DEBUG(0, ("*** mapistore_backend_get_path\n"));
		backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
		if (!backend_ctx || !backend_ctx->indexing) {
			DEBUG(0, ("Invalid backend_ctx\n"));
			goto end;
		}
		retval = mapistore_backend_get_path(NULL, backend_ctx, root_fmid, &mapistore_URI);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_backend_get_path: %s\n", mapistore_errstr(retval)));
			goto end;
		}
		talloc_free(mapistore_URI);

		retval = mapistore_backend_get_path(NULL, backend_ctx, 0xbeefdead, &mapistore_URI);
		if (retval == MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_backend_get_path: error expected!\n"));
			talloc_free(mapistore_URI);
			goto end;
		}
	}

	/* try to get first child for test Context or fallback to FMID for context folder */
	DEBUG(0, ("*** Find any children of folder with id 0x%.16"PRIx64"\n", root_fmid));
	fmid = _find_first_child_folder(mem_ctx, mstore_ctx, context_id, folder_object);
	if (!fmid) {
		/* use root context FMID, should work */
		fmid = 0xdeadbeef;
	}

	retval = mapistore_folder_open_folder(mstore_ctx, context_id, folder_object, NULL, fmid,
					      &child_folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_open_folder: %s\n", mapistore_errstr(retval)));
		goto end;
	}

	/* get_child_count */
	{
		uint32_t	count = 0;

		DEBUG(0, ("*** mapistore_folder_get_child_count\n"));
		retval = mapistore_folder_get_child_count(mstore_ctx, context_id, child_folder_object,
							  MAPISTORE_FOLDER_TABLE, &count);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_get_child_count: %s\n", mapistore_errstr(retval)));
		}
		DEBUG(0, ("mapistore_folder_get_child_count: count = %d\n", count));

		retval = mapistore_folder_get_child_count(mstore_ctx, context_id, child_folder_object,
							  MAPISTORE_MESSAGE_TABLE, &count);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_get_child_count: %s\n", mapistore_errstr(retval)));
		}
		DEBUG(0, ("mapistore_folder_get_child_count: count = %d\n", count));
	}

	/* open table */
	{
		uint32_t	count = 0;

		DEBUG(0, ("*** mapistore_folder_open_table\n"));
		retval = mapistore_folder_open_table(mstore_ctx, context_id, folder_object,
						     mem_ctx, MAPISTORE_FOLDER_TABLE, 0,
						     &table_object, &count);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_open_table: %s\n", mapistore_errstr(retval)));
			goto end;
		}
		DEBUG(0, ("open_table: count = %d\n", count));
	}

	/* set columns */
	{
		DEBUG(0, ("*** mapistore_folder_set_columns\n"));
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
						  PR_DISPLAY_NAME_UNICODE,
						  PR_FID,
						  PR_COMMENT_UNICODE,
						  PR_CONTENT_UNREAD,
						  PR_CONTENT_COUNT,
						  PR_FOLDER_CHILD_COUNT);
		retval = mapistore_table_set_columns(mstore_ctx, context_id, table_object,
						     (uint16_t)SPropTagArray->cValues,
						     SPropTagArray->aulPropTag);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_table_set_columns: %s\n", mapistore_errstr(retval)));
			goto end;
		}
	}

	/* get_row */
	{
		struct mapistore_property_data	*row_data;
		uint32_t			i;

		DEBUG(0, ("*** mapistore_table_get_row\n"));
		retval = mapistore_table_get_row(mstore_ctx, context_id, table_object,
						 mem_ctx, MAPISTORE_PREFILTERED_QUERY, 0,
						 &row_data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_table_get_row: %s\n", mapistore_errstr(retval)));
			goto end;
		}

		for (i = 0; i < SPropTagArray->cValues; i++) {
			if (row_data[i].error != MAPISTORE_SUCCESS) {
				DEBUG(0, ("0x%x: MAPI_E_NOT_FOUND\n", SPropTagArray->aulPropTag[i]));
			} else {
				struct SPropValue lpProp;

				lpProp.ulPropTag = SPropTagArray->aulPropTag[i];
				lpProp.dwAlignPad = 0;
				set_SPropValue(&lpProp, row_data[i].data);
				mapidump_SPropValue(lpProp, NULL);
			}
		}
		MAPIFreeBuffer(SPropTagArray);
		talloc_free(row_data);
	}

	/* get_row_count */
	{
		uint32_t	row_count = 0;

		DEBUG(0, ("*** mapistore_table_get_row_count\n"));
		retval = mapistore_table_get_row_count(mstore_ctx, context_id, table_object,
						       MAPISTORE_PREFILTERED_QUERY,
						       &row_count);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_table_get_row_count: %s\n", mapistore_errstr(retval)));
			goto end;
		}
		DEBUG(0, ("get_row_count: row_count = %d\n", row_count));
	}

	/* open_message */
	{
		DEBUG(0, ("*** mapistore_folder_open_message\n"));
		retval = mapistore_folder_open_message(mstore_ctx, context_id, child_folder_object, mem_ctx,
						       0xdead000100000001, 0, &message_object);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_open_message: %s\n", mapistore_errstr(retval)));
			goto end;
		}
	}

	/* get_message_data */
	{
		struct mapistore_message	*msg_data;
		struct SPropValue		lpProp;

		DEBUG(0, ("*** mapistore_message_get_message_data\n"));
		retval = mapistore_message_get_message_data(mstore_ctx, context_id,
							    message_object, mem_ctx,
							    &msg_data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_message_get_message_data: %s\n",
				  mapistore_errstr(retval)));
		}

		DEBUG(0, ("Message properties:\n"));
		DEBUG(0, (". subject_prefix     = %s\n",
			  msg_data->subject_prefix ? msg_data->subject_prefix : "Nan"));
		DEBUG(0, (". normalized_subject = %s\n",
			  msg_data->normalized_subject ? msg_data->normalized_subject : "Nan"));
		DEBUG(0, ("Recipients: (count=%d)\n", msg_data->recipients_count));
		DEBUG(0, ("---- recipient columns ----\n"));
		mapidump_SPropTagArray(msg_data->columns);
		DEBUG(0, ("---------------------------\n"));

		for (i = 0; i < msg_data->recipients_count; i++) {
			DEBUG(0, (". [%d] type     = %d\n", i, msg_data->recipients[i].type));
			DEBUG(0, (". [%d] username = %s\n", i,
				  msg_data->recipients[i].username ? msg_data->recipients[i].username : "Nan"));

			for (j = 0; j < msg_data->columns->cValues; j++) {
				lpProp.ulPropTag = msg_data->columns->aulPropTag[j];
				lpProp.dwAlignPad = 0;
				set_SPropValue(&lpProp, msg_data->recipients[i].data[j]);
				mapidump_SPropValue(lpProp, "\t.");
			}
		}
		talloc_free(msg_data);
	}

	/* get_properties */
	{
		struct mapistore_property_data	*property_data = NULL;
		uint32_t			i;

		DEBUG(0, ("*** mapistore_properties_get_properties\n"));
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
						  PidTagConversationTopic,
						  PidTagBody,
						  PidTagInstanceNum);
		property_data = talloc_array(mem_ctx, struct mapistore_property_data,
					     SPropTagArray->cValues);

		retval = mapistore_properties_get_properties(mstore_ctx, context_id,
							     message_object,
							     mem_ctx, SPropTagArray->cValues,
							     SPropTagArray->aulPropTag,
							     property_data);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_properties_get_properties: %s\n", mapistore_errstr(retval)));
			goto end;
		}

		for (i = 0; i < SPropTagArray->cValues; i++) {
			if (property_data[i].error != MAPISTORE_SUCCESS) {
				DEBUG(0, ("0x%x: MAPI_E_NOT_FOUND\n", SPropTagArray->aulPropTag[i]));
			} else {
				struct SPropValue lpProp;

				lpProp.ulPropTag = SPropTagArray->aulPropTag[i];
				lpProp.dwAlignPad = 0;
				set_SPropValue(&lpProp, property_data[i].data);
				mapidump_SPropValue(lpProp, NULL);
			}
		}

		MAPIFreeBuffer(SPropTagArray);
		talloc_free(property_data);
	}


	/* set_properties */
	{
		struct SRow		aRow;
		struct SPropValue	lpProps[2];
		uint32_t		importance = 2;
		const char		*msgid = "X-MAPISTORE_ID: 1";

		DEBUG(0, ("*** mapistore_properties_set_properties\n"));
		set_SPropValue_proptag(&lpProps[0], PidTagImportance, (const void *)(&importance));
		set_SPropValue_proptag(&lpProps[1], PidTagInternetMessageId, (const void *)msgid);

		aRow.ulAdrEntryPad = 0;
		aRow.cValues = 2;
		aRow.lpProps = lpProps;

		retval = mapistore_properties_set_properties(mstore_ctx, context_id,
							     message_object, &aRow);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_properties_set_properties: %s\n",
				  mapistore_errstr(retval)));
			goto end;
		}


		importance = 0;
		set_SPropValue_proptag(&lpProps[0], PidTagSensitivity, (const void *)(&importance));
		msgid = "X-MAPISTORE_ID: 2";
		set_SPropValue_proptag(&lpProps[1], PidTagInternetMessageId, (const void *)msgid);
		retval = mapistore_properties_set_properties(mstore_ctx, context_id,
							     message_object, &aRow);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_properties_set_properties: %s\n",
				  mapistore_errstr(retval)));
			goto end;
		}
	}

	/* create_message / set_properties / child_count / savechangesmessage / child_count */
	{
		void			*newmsg_object;
		struct SRow		aRow;
		struct SPropValue	lpProps[2];
		const char		*subject = "Sample subject for 0xdead0002";
		const char		*body = "Hello world!";
		uint32_t		count = 0;

		DEBUG(0, ("*** mapistore_folder_create_message\n"));
		retval = mapistore_folder_create_message(mstore_ctx, context_id,
							 child_folder_object, mem_ctx,
							 0xdead0002, false, &newmsg_object);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_create_message: %s\n",
				  mapistore_errstr(retval)));
			goto end;
		}

		DEBUG(0, ("** mapistore_properties_set_properties\n"));
		set_SPropValue_proptag(&lpProps[0], PidTagSubject, (const void *)subject);
		set_SPropValue_proptag(&lpProps[1], PidTagBody, (const void *)body);

		aRow.ulAdrEntryPad = 0;
		aRow.cValues = 2;
		aRow.lpProps = lpProps;

		retval = mapistore_properties_set_properties(mstore_ctx, context_id,
							     newmsg_object, &aRow);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_properties_set_properties: %s\n",
				  mapistore_errstr(retval)));
			goto end;
		}

		DEBUG(0, ("** mapistore_folder_get_child_count\n"));
		retval = mapistore_folder_get_child_count(mstore_ctx, context_id, child_folder_object,
							  MAPISTORE_MESSAGE_TABLE, &count);

		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_get_child_count: %s\n", mapistore_errstr(retval)));
		}
		DEBUG(0, ("mapistore_folder_get_child_count: count = %d\n", count));

		DEBUG(0, ("** mapistore_message_save\n"));
		retval = mapistore_message_save(mstore_ctx, context_id, newmsg_object, mem_ctx);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_message_save: %s\n", mapistore_errstr(retval)));
			goto end;
		}

		DEBUG(0, ("** mapistore_folder_get_child_count\n"));
		retval = mapistore_folder_get_child_count(mstore_ctx, context_id, child_folder_object,
							  MAPISTORE_MESSAGE_TABLE, &count);

		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_folder_get_child_count: %s\n", mapistore_errstr(retval)));
		}
		DEBUG(0, ("mapistore_folder_get_child_count: count = %d\n", count));
	}

	/* Test Folder Create/Delete */
	retval = _test_folder_create_delete(mem_ctx, mstore_ctx, context_id, folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("_test_folder_create_delete: %s\n", mapistore_errstr(retval)));
		goto end;
	}

 end:
	retval = mapistore_del_context(mstore_ctx, context_id);
	retval = mapistore_release(mstore_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("%s\n", mapistore_errstr(retval)));
		goto end;
	}

	talloc_free(mem_ctx);
	return 0;
}
