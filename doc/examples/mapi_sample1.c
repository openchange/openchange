#include <libmapi/libmapi.h>

#define   DEFAULT_PROFDB_PATH     "%s/.openchange/profiles.ldb"

int main(int argc, char *argv[])
{
        TALLOC_CTX              *mem_ctx;
        enum MAPISTATUS         retval;
        struct mapi_session     *session = NULL;
        char                    *profdb;
        char                    *profname;

        mem_ctx = talloc_named(NULL, 0, "mapi_sample1");

        profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB_PATH, getenv("HOME"));

        retval = MAPIInitialize(profdb);
        mapi_errstr("MAPIInitialize", GetLastError());
        if (retval != MAPI_E_SUCCESS) return -1;

	retval = GetDefaultProfile(&profname);
        mapi_errstr("GetDefaultProfile", GetLastError());
	if (retval != MAPI_E_SUCCESS) return -1;

	retval = MapiLogonEx(&session, profname, NULL);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return -1;

        MAPIUninitialize();
        talloc_free(mem_ctx);

        return 0;
}
