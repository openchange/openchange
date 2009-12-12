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

int ocpf_yyparse(void);

struct ocpf	*ocpf;
extern FILE	*yyin;
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
	
	if (ocpf) return OCPF_ERROR;

	mem_ctx = talloc_named(NULL, 0, "ocpf");
	ocpf = talloc_zero(mem_ctx, struct ocpf);
	ocpf->mem_ctx = mem_ctx;
	ocpf->vars = talloc_zero(mem_ctx, struct ocpf_var);
	ocpf->oleguid = talloc_zero(mem_ctx, struct ocpf_oleguid);
	ocpf->props = talloc_zero(mem_ctx, struct ocpf_property);
	ocpf->nprops = talloc_zero(mem_ctx, struct ocpf_nproperty);
	ocpf->recipients = talloc_zero(mem_ctx, struct ocpf_recipients);
	ocpf->lpProps = NULL;
	ocpf->filename = NULL;
	ocpf->cValues = 0;
	ocpf->folder = 0;
	
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
	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;

	talloc_free(ocpf->mem_ctx);
	ocpf = NULL;

	return OCPF_SUCCESS;
}


/**
   \details Parse OCPF file

   Parse and process the given ocpf file.

   \param filename the file to parse

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_init
 */
_PUBLIC_ int ocpf_parse(const char *filename)
{
	int		ret;
	struct stat	sb;

	if (!filename) return OCPF_ERROR;
	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;

	ocpf->filename = filename;
	lineno = 1;

	/* Sanity check on filename */
	OCPF_RETVAL_IF((!filename || (stat(filename, &sb) == -1)), 
		       OCPF_WARN_FILENAME_INVALID, NULL);

	yyin = fopen(filename, "r");
	OCPF_RETVAL_IF(yyin == NULL, OCPF_WARN_FILENAME_INVALID, NULL);

	ret = ocpf_yyparse();
	fclose(yyin);

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
		OCPF_RETVAL_IF(retval, retval, NULL);

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

   \param mem_ctx the memory context to use for memory allocation
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
					     mapi_object_t *obj_folder,
					     mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
	struct mapi_nameid	*nameid;
	struct SPropTagArray	*SPropTagArray;
	struct ocpf_property	*pel;
	struct ocpf_nproperty	*nel;
	uint32_t		i;

	/* sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Allocate SPropValue */
	ocpf->cValues = 0;
	ocpf->lpProps = talloc_array(mem_ctx, struct SPropValue, 2);

	/* Step2. build the list of named properties we want to set */
	if (ocpf->nprops && ocpf->nprops->next) {
		nameid = mapi_nameid_new(mem_ctx);
		for (nel = ocpf->nprops; nel->next; nel = nel->next) {
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
		for (nel = ocpf->nprops, i = 0; SPropTagArray->aulPropTag[i] && nel->next; nel = nel->next, i++) {
			if (SPropTagArray->aulPropTag[i]) {
				if (((SPropTagArray->aulPropTag[i] & 0xFFFF) == PT_BINARY) && 
				    (((struct Binary_r *)nel->value)->cb > MAX_READ_SIZE)) {
					retval = ocpf_stream(mem_ctx, obj_message, SPropTagArray->aulPropTag[i], 
							     (struct Binary_r *)nel->value);
					MAPI_RETVAL_IF(retval, retval, NULL);
				} else {
					ocpf->lpProps = add_SPropValue(mem_ctx, ocpf->lpProps, &ocpf->cValues,
								       SPropTagArray->aulPropTag[i], nel->value);
				}
			}
		}
		MAPIFreeBuffer(SPropTagArray);
	}

	/* Step5. Add Known properties */
	if (ocpf->props && ocpf->props->next) {
		for (pel = ocpf->props; pel->next; pel = pel->next) {
			if (((pel->aulPropTag & 0xFFFF) == PT_BINARY) && 
			    (((struct Binary_r *)pel->value)->cb > MAX_READ_SIZE)) {
				retval = ocpf_stream(mem_ctx, obj_message, pel->aulPropTag, 
						     (struct Binary_r *)pel->value);
				MAPI_RETVAL_IF(retval, retval, NULL);
			} else {
				ocpf->lpProps = add_SPropValue(mem_ctx, ocpf->lpProps, &ocpf->cValues, 
							       pel->aulPropTag, pel->value);
			}
		}
	}
	/* Step 6. Add message class */
	if (ocpf->type) {
		ocpf->lpProps = add_SPropValue(mem_ctx, ocpf->lpProps, &ocpf->cValues,
					       PR_MESSAGE_CLASS, (const void *)ocpf->type);
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Get the OCPF SPropValue array

   This function is an accessor designed to return the SPropValue
   structure created with ocpf_set_SPropValue.

   \param cValues pointer on the number of SPropValue entries

   \return NULL on error, otherwise returns an allocated lpProps pointer

   \sa ocpf_set_SPropValue
 */
_PUBLIC_ struct SPropValue *ocpf_get_SPropValue(uint32_t *cValues)
{
	if (!ocpf || !ocpf->lpProps) return NULL;
	if (!ocpf->cValues) return NULL;

	*cValues = ocpf->cValues;

	return ocpf->lpProps;
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
_PUBLIC_ enum MAPISTATUS ocpf_OpenFolder(mapi_object_t *obj_store,
					 mapi_object_t *obj_folder)
{
	enum MAPISTATUS	retval;
	mapi_id_t	id_folder;
	mapi_id_t	id_tis;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);

	MAPI_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!ocpf->folder, MAPI_E_NOT_FOUND, NULL);

	/* */
	mapi_object_init(obj_folder);
	if (ocpf->folder >= 1 && ocpf->folder <= 26) {
		retval = GetDefaultFolder(obj_store, &id_folder, ocpf->folder);
		MAPI_RETVAL_IF(retval, retval, NULL);

		retval = OpenFolder(obj_store, id_folder, obj_folder);
		MAPI_RETVAL_IF(retval, retval, NULL);

	} else {
		retval = GetDefaultFolder(obj_store, &id_tis, olFolderTopInformationStore);
		MAPI_RETVAL_IF(retval, retval, NULL);

		retval = ocpf_folder_lookup((TALLOC_CTX *)ocpf->mem_ctx, ocpf->folder, 
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
   \param obj_message pointer to the message object we use for
   internal MAPI operations

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR.

   \sa ocpf
 */
_PUBLIC_ enum MAPISTATUS ocpf_set_Recipients(TALLOC_CTX *mem_ctx,
					     mapi_object_t *obj_message)
{
	enum MAPISTATUS		retval;
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
	MAPI_RETVAL_IF(!ocpf->recipients->next, MAPI_E_NOT_FOUND, NULL);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	/* Step 1. Group recipients and run ResolveNames */
	usernames = talloc_array(mem_ctx, char *, 2);
	recipClass = talloc_array(mem_ctx, int, 2);
	for (element = ocpf->recipients, count = 0; element->next; element = element->next, count ++) {
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
