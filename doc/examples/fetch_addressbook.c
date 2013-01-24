#include <libmapi/libmapi.h>

#define DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

int main(int argc, char *argv[])
{
	enum MAPISTATUS                 retval;
	struct mapi_context		*mapi_ctx;
	TALLOC_CTX			*mem_ctx;
	struct mapi_session             *session = NULL;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyRowSet_r		*srowset;
	mapi_id_t                       id_inbox;
	mapi_id_t                       *fid, *mid;
	char 				*profname;
	char				*profdb;
	uint32_t                        i;
	uint32_t			count = 1000;
	struct nspi_context		*nspi_ctx = NULL;
	struct PropertyTagArray_r	*MIds = NULL;
	struct PropertyValue_r		*lpProp = NULL;
	struct Restriction_r		Filter;

	mem_ctx = talloc_named(NULL, 0, "fetch_addressbook");

	/* Initialize MAPI */
	profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	retval = MAPIInitialize(&mapi_ctx, profdb);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* Find Default Profile */
	retval = GetDefaultProfile(mapi_ctx, &profname);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* Set up some debugging if you want to see more details */
	/* SetMAPIDumpData(mapi_ctx, true); */
	/* SetMAPIDebugLevel(mapi_ctx, 6); */

	/* Log on EMSMDB and NSPI */
	retval = MapiLogonEx(mapi_ctx, &session, profname, NULL);
	MAPI_RETVAL_IF(retval, retval, NULL);

	nspi_ctx = (struct nspi_context *)session->nspi->ctx;
	nspi_ctx->pStat->CurrentRec = 0;
	nspi_ctx->pStat->Delta = 0;
	nspi_ctx->pStat->NumPos = 0;
	nspi_ctx->pStat->TotalRecs = 0xffffffff;

	srowset = talloc_zero(mem_ctx, struct PropertyRowSet_r);

	/* build the filter - in this case, display names that start with the command line argument, or a default of "te". case insensitive match) */
	lpProp = talloc_zero(mem_ctx, struct PropertyValue_r);
	lpProp->ulPropTag = PR_DISPLAY_NAME_UNICODE;
	lpProp->dwAlignPad = 0;
	if (argc > 1) {
		lpProp->value.lpszW = argv[1];
	} else {
		lpProp->value.lpszW = "te";
	}

	Filter.rt = RES_CONTENT;
	Filter.res.resContent.ulFuzzyLevel = (FL_LOOSE << 16) | FL_PREFIX;
	Filter.res.resContent.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	Filter.res.resContent.lpProp = lpProp;

	/* construct an explicit table */
	MIds = talloc_zero(mem_ctx, struct PropertyTagArray_r);
	retval = nspi_GetMatches(nspi_ctx, mem_ctx, NULL, &Filter, 5000, &srowset, &MIds);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* fetch the contents of the explicit table */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc,
					  PR_INSTANCE_KEY,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_DISPLAY_TYPE,
					  PR_OBJECT_TYPE,
					  PR_ADDRTYPE_UNICODE,
					  PR_OFFICE_TELEPHONE_NUMBER_UNICODE,
					  PR_OFFICE_LOCATION_UNICODE,
					  PR_TITLE_UNICODE,
					  PR_COMPANY_NAME_UNICODE,
					  PR_ACCOUNT_UNICODE);

	retval = nspi_QueryRows(nspi_ctx, mem_ctx, SPropTagArray, MIds, count, &srowset);
	MAPI_RETVAL_IF(retval, retval, NULL);

	/* Show the contents */
	for (i = 0; i < srowset->cRows; ++i) {
		mapidump_PAB_entry(&srowset->aRow[i]);
	}

	/* Uninitialize MAPI */
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
