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

/**
   \file ocpf_public.c

   \brief public OCPF API
 */

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>

#include <sys/stat.h>

int ocpf_yylex_init(void *);
int ocpf_yylex_init_extra(struct ocpf_context *, void *);
void ocpf_yyset_in(FILE *, void *);
int ocpf_yylex_destroy(void *);
int ocpf_yyparse(struct ocpf_context *, void *);

struct ocpf	*ocpf;
int		error_flag;


/**
   \details Initialize OCPF context

   Initialize ocpf context and allocate memory for internal structures

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_release, ocpf_parse
 */
_PUBLIC_ int ocpf_init(void)
{
	TALLOC_CTX	*mem_ctx;
	
	OCPF_RETVAL_IF(ocpf, NULL, OCPF_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "ocpf");
	ocpf = talloc_zero(mem_ctx, struct ocpf);
	ocpf->mem_ctx = mem_ctx;

	ocpf->context = talloc_zero(mem_ctx, struct ocpf_context);
	ocpf->free_id = talloc_zero(mem_ctx, struct ocpf_freeid);
	ocpf->last_id = 1;

	return OCPF_SUCCESS;
}


/**
   \details Uninitialize OCPF context

   Uninitialize the global OCPF context and release memory.

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_init
 */
_PUBLIC_ int ocpf_release(void)
{
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);	

	talloc_free(ocpf->mem_ctx);
	ocpf = NULL;

	return OCPF_SUCCESS;
}


/**
   \details Create a new OCPF context

   \param filename the filename to process
   \param context_id pointer to the context identifier the function
   \param flags Flags controlling how the OCPF should be opened

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
_PUBLIC_ int ocpf_new_context(const char *filename, uint32_t *context_id, uint8_t flags)
{
	struct ocpf_context	*ctx;
	bool			existing = false;

	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);

	ctx = ocpf_context_add(ocpf, filename, context_id, flags, &existing);
	if (!ctx) {
		return OCPF_ERROR;
	}

	if (existing == false) {
		DLIST_ADD_END(ocpf->context, ctx, struct ocpf_context *);
		return OCPF_SUCCESS;
	} 

	return OCPF_E_EXIST;
}


/**
   \details Delete an OCPF context

   \param context_id context identifier referencing the context to
   delete

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
_PUBLIC_ int ocpf_del_context(uint32_t context_id)
{
	int			ret;
	struct ocpf_context	*ctx;

	/* Sanity checks */
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);

	/* Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	ret = ocpf_context_delete(ocpf, ctx);
	if (ret == -1) return OCPF_ERROR;

	return OCPF_SUCCESS;
}


/**
   \details Parse OCPF file

   Parse and process the given ocpf file.

   \param context_id the identifier of the context holding the file to
   be parsed

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_init
 */
_PUBLIC_ int ocpf_parse(uint32_t context_id)
{
	int			ret;
	struct ocpf_context	*ctx;
	void			*scanner;

	/* Sanity checks */
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);

	/* Step 1. Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	ret = ocpf_yylex_init(&scanner);
	ret = ocpf_yylex_init_extra(ctx, &scanner);
	ocpf_yyset_in(ctx->fp, scanner);
	ret = ocpf_yyparse(ctx, scanner);
	ocpf_yylex_destroy(scanner);

	return ret;
}


#define	MAX_READ_SIZE	0x1000

static enum MAPISTATUS ocpf_stream(TALLOC_CTX *mem_ctx,
				   mapi_object_t *obj_parent,
				   uint32_t aulPropTag,
				   struct Binary_r *bin)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_stream;
	DATA_BLOB		stream;
	uint32_t		access_flags = 2;	/* MAPI_MODIFY by default */
	uint32_t		size;
	uint32_t		offset;
	uint16_t		read_size;

	mapi_object_init(&obj_stream);

	/* Step1. Open the Stream */
	retval = OpenStream(obj_parent, aulPropTag, access_flags, &obj_stream);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* Step2. Write the Stream */
	size = MAX_READ_SIZE;
	offset = 0;
	while (offset <= bin->cb) {
		stream.length = size;
		stream.data = talloc_size(mem_ctx, size);
		memcpy(stream.data, bin->lpb + offset, size);
		
		retval = WriteStream(&obj_stream, &stream, &read_size);
		talloc_free(stream.data);
		MAPI_RETVAL_IF(retval, retval, NULL);

		/* Exit when there is nothing left to write */
		if (!read_size) return MAPI_E_SUCCESS;
		
		offset += read_size;

		if ((offset + size) > bin->cb) {
			size = bin->cb - offset;
		}
	}

	mapi_object_release(&obj_stream);

	return MAPI_E_SUCCESS;
}


/**
   \details Build a SPropValue array from ocpf context

   This function builds a SPropValue array from the ocpf context and
   information stored.
   
   \param mem_ctx pointer to the memory context to use for memory
   allocation
   \param context_id identifier of the context to build a SPropValue
   array for

   \note This function is a server-side convenient function only. It
   doesn't handle named properties and its scope is much more limited
   than ocpf_set_SpropValue. Developers working on a client-side
   software/library must use ocpf_set_SPropValue instead.

   \sa ocpf_get_SPropValue
 */
_PUBLIC_ enum MAPISTATUS ocpf_set_SPropValue_array(TALLOC_CTX *mem_ctx, uint32_t context_id)
{
	struct ocpf_property	*pel;
	struct ocpf_context	*ctx;

	/* sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	/* Step 2. Allocate SPropValue */
	ctx->cValues = 0;
	ctx->lpProps = talloc_array(ctx, struct SPropValue, 2);

	/* Step 3. Add Known properties */
	if (ctx->props && ctx->props->next) {
		for (pel = ctx->props; pel->next; pel = pel->next) {
			ctx->lpProps = add_SPropValue(ctx, ctx->lpProps, &ctx->cValues, 
						      pel->aulPropTag, pel->value);
		}
	}
	/* Step 4. Add message class */
	if (ctx->type) {
		ctx->lpProps = add_SPropValue(ctx, ctx->lpProps, &ctx->cValues,
					      PR_MESSAGE_CLASS, (const void *)ctx->type);
	}
	
	return MAPI_E_SUCCESS;
}


/**
   \details Build a SRowSet array with recipients from ocpf context

   This function builds s SRowSet structure of recipient names and
   type from the ocpf context and information stored.

   \param mem_ctx pointer to the memory context to use for memory
   allocation
   \param context_id identifier of the context to use for building a
   SRowSet array of recipients

   \return Pointer to an allocated SRowSet structure on success,
   otherwise NULL
 */
_PUBLIC_ struct SRowSet *ocpf_get_recipients(TALLOC_CTX *mem_ctx,
					  uint32_t context_id)
{
	struct SRowSet		*SRowSet;
	struct ocpf_context	*ctx;
	struct ocpf_recipients	*recipient;
	int			i;

	/* Sanity checks */
	if (!ocpf) return NULL;

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return NULL;

	/* Step 2. Allocate SRow */
	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	SRowSet->cRows = 0;

	/* Count the number of recipients and allocate memory for aRow */
	for (recipient = ctx->recipients; recipient->next; recipient = recipient->next) {
		SRowSet->cRows += 1;
	}
	SRowSet->aRow = talloc_array(SRowSet, struct SRow, SRowSet->cRows + 1);

	for (i = 0, recipient = ctx->recipients; recipient->next; recipient = recipient->next, i++) {
		SRowSet->aRow[i].ulAdrEntryPad = 0;
		SRowSet->aRow[i].cValues = 2;
		SRowSet->aRow[i].lpProps = talloc_array(SRowSet->aRow, struct SPropValue, 3);
		
		SRowSet->aRow[i].lpProps[0].ulPropTag = PR_RECIPIENT_TYPE;
		SRowSet->aRow[i].lpProps[0].value.l = recipient->class;
		
		SRowSet->aRow[i].lpProps[1].ulPropTag = PR_DISPLAY_NAME;
		SRowSet->aRow[i].lpProps[1].value.lpszA = talloc_strdup(SRowSet->aRow[i].lpProps, 
									recipient->name);
	}

	return SRowSet;
}


/**
   \details Build a SPropValue array from ocpf context

   This function builds a SPropValue array from the ocpf context and
   information stored.

   \param mem_ctx the memory context to use for memory allocation
   \param context_id identifier of the context to build a SPropValue
   array for
   \param obj_folder pointer the folder object we use for internal
   MAPI operations
   \param obj_message pointer to the message object we use for
   internal MAPI operations

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized

   \sa ocpf_get_SPropValue
 */

_PUBLIC_ enum MAPISTATUS ocpf_set_SPropValue(TALLOC_CTX *mem_ctx,
					     uint32_t context_id,
					     mapi_object_t *obj_folder,
					     mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	struct ocpf_property	*pel;
	struct ocpf_nproperty	*nel;
	struct ocpf_context	*ctx;
	uint32_t		i;

	/* sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	
	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	/* Step 1. Allocate SPropValue */
	ctx->cValues = 0;
	ctx->lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	/* Step2. build the list of named properties we want to set */
	if (ctx->nprops && ctx->nprops->next) {
		nameid = mapi_nameid_new(mem_ctx);
		for (nel = ctx->nprops; nel->next; nel = nel->next) {
			if (nel->OOM) {
				mapi_nameid_OOM_add(nameid, nel->OOM, nel->oleguid);
			} else if (nel->mnid_id) {
				mapi_nameid_custom_lid_add(nameid, nel->mnid_id, nel->propType, nel->oleguid);
			} else if (nel->mnid_string) {
				mapi_nameid_custom_string_add(nameid, nel->mnid_string, nel->propType, nel->oleguid);
			}
		}
		
		/* Step3. GetIDsFromNames and map property types */
		SPropTagArray = talloc_zero(mem_ctx, struct SPropTagArray);
		retval = GetIDsFromNames(obj_folder, nameid->count, 
					 nameid->nameid, 0, &SPropTagArray);
		if (retval != MAPI_E_SUCCESS) {
			MAPIFreeBuffer(SPropTagArray);
			MAPIFreeBuffer(nameid);
			return retval;
		}
		mapi_nameid_SPropTagArray(nameid, SPropTagArray);
		MAPIFreeBuffer(nameid);


		/* Step4. Add named properties */
		for (nel = ctx->nprops, i = 0; SPropTagArray->aulPropTag[i] && nel->next; nel = nel->next, i++) {
			if (SPropTagArray->aulPropTag[i]) {
				if (((SPropTagArray->aulPropTag[i] & 0xFFFF) == PT_BINARY) && 
				    (((struct Binary_r *)nel->value)->cb > MAX_READ_SIZE)) {
					retval = ocpf_stream(mem_ctx, obj_message, SPropTagArray->aulPropTag[i], 
							     (struct Binary_r *)nel->value);
					MAPI_RETVAL_IF(retval, retval, NULL);
				} else {
					ctx->lpProps = add_SPropValue(mem_ctx, ctx->lpProps, &ctx->cValues,
								       SPropTagArray->aulPropTag[i], nel->value);
				}
			}
		}
		MAPIFreeBuffer(SPropTagArray);
	}

	/* Step5. Add Known properties */
	if (ctx->props && ctx->props->next) {
		for (pel = ctx->props; pel->next; pel = pel->next) {
			if (((pel->aulPropTag & 0xFFFF) == PT_BINARY) && 
			    (((struct Binary_r *)pel->value)->cb > MAX_READ_SIZE)) {
				retval = ocpf_stream(mem_ctx, obj_message, pel->aulPropTag, 
						     (struct Binary_r *)pel->value);
				MAPI_RETVAL_IF(retval, retval, NULL);
			} else {
				ctx->lpProps = add_SPropValue(mem_ctx, ctx->lpProps, &ctx->cValues, 
							       pel->aulPropTag, pel->value);
			}
		}
	}
	/* Step 6. Add message class */
	if (ctx->type) {
		ctx->lpProps = add_SPropValue(mem_ctx, ctx->lpProps, &ctx->cValues,
					       PR_MESSAGE_CLASS, (const void *)ctx->type);
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Get the OCPF SPropValue array

   This function is an accessor designed to return the SPropValue
   structure created with ocpf_set_SPropValue.

   \param context_id identifier of the context to retrieve SPropValue
   from
   \param cValues pointer on the number of SPropValue entries

   \return NULL on error, otherwise returns an allocated lpProps pointer

   \sa ocpf_set_SPropValue
 */
_PUBLIC_ struct SPropValue *ocpf_get_SPropValue(uint32_t context_id, uint32_t *cValues)
{
	struct ocpf_context	*ctx;

	OCPF_RETVAL_TYPE(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL, NULL);

	/* Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_TYPE(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL, NULL);

	OCPF_RETVAL_TYPE(!ctx->lpProps || !ctx->cValues, ctx, OCPF_INVALID_PROPARRAY, NULL, NULL);

	*cValues = ctx->cValues;

	return ctx->lpProps;
}


static enum MAPISTATUS ocpf_folder_lookup(TALLOC_CTX *mem_ctx,
					  uint64_t sfid,
					  mapi_object_t *obj_parent,
					  mapi_id_t folder_id,
					  mapi_object_t *obj_ret)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		i;
	const uint64_t		*fid;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(obj_parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_FID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;

	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND && SRowSet.cRows)) {
		for (i = 0; i < SRowSet.cRows; i++) {
			fid = (const uint64_t *)find_SPropValue_data(&SRowSet.aRow[i], PR_FID);
			if (fid && *fid == sfid) {
				retval = OpenFolder(&obj_folder, *fid, obj_ret);
				mapi_object_release(&obj_htable);
				mapi_object_release(&obj_folder);
				return MAPI_E_SUCCESS;
			} else if (fid) {
				retval = ocpf_folder_lookup(mem_ctx, sfid, &obj_folder, *fid, obj_ret);
				if (retval == MAPI_E_SUCCESS) {
					mapi_object_release(&obj_htable);
					mapi_object_release(&obj_folder);
					return MAPI_E_SUCCESS;
				}
			}
		}
	}

	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);

	errno = MAPI_E_NOT_FOUND;
	return MAPI_E_NOT_FOUND;
}


/**
   \details Open OCPF folder

   This function opens the folder associated with the ocpf folder
   global context value.
   
   \param context_id identifier of the context to open the folder for
   \param obj_store the store object
   \param obj_folder the folder to open

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_NOT_FOUND: The specified folder could not be found or is
     not yet supported.

     \sa ocpf_init, ocpf_parse
 */
_PUBLIC_ enum MAPISTATUS ocpf_OpenFolder(uint32_t context_id,
					 mapi_object_t *obj_store,
					 mapi_object_t *obj_folder)
{
	enum MAPISTATUS		retval;
	struct ocpf_context	*ctx;
	mapi_id_t		id_folder;
	mapi_id_t		id_tis;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	MAPI_RETVAL_IF(!ctx, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!ctx->folder, MAPI_E_NOT_FOUND, NULL);

	mapi_object_init(obj_folder);
	if (ctx->folder >= 1 && ctx->folder <= 26) {
		retval = GetDefaultFolder(obj_store, &id_folder, ctx->folder);
		MAPI_RETVAL_IF(retval, retval, NULL);

		retval = OpenFolder(obj_store, id_folder, obj_folder);
		MAPI_RETVAL_IF(retval, retval, NULL);

	} else {
		retval = GetDefaultFolder(obj_store, &id_tis, olFolderTopInformationStore);
		MAPI_RETVAL_IF(retval, retval, NULL);

		retval = ocpf_folder_lookup((TALLOC_CTX *)ctx, ctx->folder, 
					    obj_store, id_tis, obj_folder);
		MAPI_RETVAL_IF(retval, retval, NULL);
	}

	return MAPI_E_SUCCESS;
}


/**
 * We set external recipients at the end of aRow
 */
static bool set_external_recipients(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, const char *username, enum ulRecipClass RecipClass)
{
	uint32_t		last;
	struct SPropValue	SPropValue;

	SRowSet->aRow = talloc_realloc(mem_ctx, SRowSet->aRow, struct SRow, SRowSet->cRows + 2);
	last = SRowSet->cRows;
	SRowSet->aRow[last].cValues = 0;
	SRowSet->aRow[last].lpProps = talloc_zero(mem_ctx, struct SPropValue);
	
	/* PR_OBJECT_TYPE */
	SPropValue.ulPropTag = PR_OBJECT_TYPE;
	SPropValue.value.l = MAPI_MAILUSER;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_TYPE */
	SPropValue.ulPropTag = PR_DISPLAY_TYPE;
	SPropValue.value.l = 0;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_GIVEN_NAME */
	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_7BIT_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_7BIT_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_SMTP_ADDRESS */
	SPropValue.ulPropTag = PR_SMTP_ADDRESS;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_ADDRTYPE */
	SPropValue.ulPropTag = PR_ADDRTYPE;
	SPropValue.value.lpszA = "SMTP";
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	SetRecipientType(&(SRowSet->aRow[last]), RecipClass);

	SRowSet->cRows += 1;
	return true;
}


/**
   \details Set the message recipients from ocpf context

   This function sets the recipient (To, Cc, Bcc) from the ocpf
   context and information stored.

   \param mem_ctx the memory context to use for memory allocation
   \param context_id identifier to the context to set recipients for
   \param obj_message pointer to the message object we use for
   internal MAPI operations

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR.

   \sa ocpf
 */
_PUBLIC_ enum MAPISTATUS ocpf_set_Recipients(TALLOC_CTX *mem_ctx,
					     uint32_t context_id,
					     mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
	struct ocpf_context	*ctx;
	struct ocpf_recipients	*element;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	SPropValue;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	char			**usernames = NULL;
	int			*recipClass = NULL;
	uint32_t		count;
	uint32_t		counter;
	uint32_t		i;

	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	MAPI_RETVAL_IF(!ctx, MAPI_E_INVALID_PARAMETER, NULL);

	MAPI_RETVAL_IF(!ctx->recipients->next, MAPI_E_NOT_FOUND, NULL);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x8,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME,
					  PR_EMAIL_ADDRESS,
					  PR_ADDRTYPE);

	/* Step 1. Group recipients and run ResolveNames */
	usernames = talloc_array(mem_ctx, char *, 2);
	recipClass = talloc_array(mem_ctx, int, 2);
	for (element = ctx->recipients, count = 0; element->next; element = element->next, count ++) {
		usernames = talloc_realloc(mem_ctx, usernames, char *, count + 2);
		recipClass = talloc_realloc(mem_ctx, recipClass, int, count + 2);
		usernames[count] = talloc_strdup((TALLOC_CTX *)usernames, element->name);
		recipClass[count] = element->class;
	}
	usernames[count] = 0;

	retval = ResolveNames(mapi_object_get_session(obj_message), (const char **)usernames, 
			      SPropTagArray, &SRowSet, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, usernames);

	/* Step2. Associate resolved recipients to their respective recipClass */
	if (!SRowSet) {
		SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	}

	count = 0;
	counter = 0;
	for (i = 0; usernames[i]; i++) {
		if (flaglist->aulPropTag[count] == MAPI_UNRESOLVED) {
			set_external_recipients(mem_ctx, SRowSet, usernames[i], recipClass[i]);
		}
		if (flaglist->aulPropTag[count] == MAPI_RESOLVED) {
			SetRecipientType(&(SRowSet->aRow[counter]), recipClass[i]);
			counter++;
		}
		count++;
	}

	/* Step3. Finish to build the ModifyRecipients SRowSet */
	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mem_ctx, SRowSet, SPropValue);

	/* Step4. Call ModifyRecipients */
	retval = ModifyRecipients(obj_message, SRowSet);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	MAPIFreeBuffer(usernames);
	MAPI_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}
