#include <libmapi/libmapi.h>

#define DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

int main(int argc, char *argv[])
{
        enum MAPISTATUS                 retval;
	struct mapi_context		*mapi_ctx;
	TALLOC_CTX			*mem_ctx;
        struct mapi_session             *session = NULL;
        mapi_object_t                   obj_store;
        mapi_object_t                   obj_folder;
        mapi_object_t                   obj_table;
        mapi_object_t                   obj_message;
        struct mapi_SPropValue_array	props_all;
        struct SRowSet                  rowset;
        struct SPropTagArray            *SPropTagArray;
        mapi_id_t                       id_inbox;
        mapi_id_t                       *fid, *mid;
        char 				*profname;
	char				*profdb;
	uint32_t			Numerator;
	uint32_t			Denominator;
        uint32_t                        i;

	mem_ctx = talloc_named(NULL, 0, "fetchappointment");

        /* Initialize MAPI */
	profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
        retval = MAPIInitialize(&mapi_ctx, profdb);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Find Default Profile */
        retval = GetDefaultProfile(mapi_ctx, &profname);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Log on EMSMDB and NSPI */
        retval = MapiLogonEx(mapi_ctx, &session, profname, NULL);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Open Message Store */
        mapi_object_init(&obj_store);
        retval = OpenMsgStore(session, &obj_store);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Find Inbox default folder */
        retval = GetDefaultFolder(&obj_store, &id_inbox, olFolderCalendar);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Open Inbox folder */
        mapi_object_init(&obj_folder);
        retval = OpenFolder(&obj_store, id_inbox, &obj_folder);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Retrieve Inbox content table */
        mapi_object_init(&obj_table);
        retval = GetContentsTable(&obj_folder, &obj_table, 0x0, NULL);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Create the MAPI table view */
        SPropTagArray = set_SPropTagArray(mem_ctx, 0x2, PR_FID, PR_MID);
        retval = SetColumns(&obj_table, SPropTagArray);
        MAPIFreeBuffer(SPropTagArray);
        MAPI_RETVAL_IF(retval, retval, mem_ctx);

        /* Get current cursor position */
        retval = QueryPosition(&obj_table, &Numerator, &Denominator);
        MAPI_RETVAL_IF(retval, retval, NULL);

        /* Iterate through rows */
        while ((retval = QueryRows(&obj_table, Denominator, TBL_ADVANCE, &rowset)) != -1 && rowset.cRows) {
                for (i = 0; i < rowset.cRows; i++) {
			fid = (mapi_id_t *)find_SPropValue_data(&(rowset.aRow[i]), PR_FID);
			mid = (mapi_id_t *)find_SPropValue_data(&(rowset.aRow[i]), PR_MID);
			mapi_object_init(&obj_message);
                        retval = OpenMessage(&obj_store, *fid, *mid, &obj_message, 0x0);
                        if (retval != MAPI_E_NOT_FOUND) {
                                retval = GetPropsAll(&obj_message, MAPI_UNICODE, &props_all);
                                mapidump_appointment(&props_all, NULL);
                                mapi_object_release(&obj_message);
                        }
                }

        }

        /* Release MAPI objects */
        mapi_object_release(&obj_table);
        mapi_object_release(&obj_folder);

	Logoff(&obj_store);

        /* Uninitialize MAPI */
        MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

        return (0);
}
