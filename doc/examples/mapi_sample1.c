#include <libmapi/libmapi.h>

#define   DEFAULT_PROFDB_PATH     "%s/.openchange/profiles.ldb"

int main(int argc, char *argv[])
{
        TALLOC_CTX              *mem_ctx;
	struct mapi_context	*mapi_ctx;
        enum MAPISTATUS         retval;
        struct mapi_session     *session = NULL;
        char                    *profdb;
        char                    *profname;

        mem_ctx = talloc_named(NULL, 0, "mapi_sample1");

        profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB_PATH, getenv("HOME"));

        retval = MAPIInitialize(&mapi_ctx, profdb);
        mapi_errstr("MAPIInitialize", GetLastError());
        if (retval != MAPI_E_SUCCESS) return -1;

	retval = GetDefaultProfile(mapi_ctx, &profname);
        mapi_errstr("GetDefaultProfile", GetLastError());
	if (retval != MAPI_E_SUCCESS) return -1;

	retval = MapiLogonEx(mapi_ctx, &session, profname, NULL);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return -1;

        MAPIUninitialize(mapi_ctx);
        talloc_free(mem_ctx);

        return 0;
}
