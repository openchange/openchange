#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mapi.h"

// from openchange
#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"

static zend_function_entry mapi_functions[] = {
    PHP_FE(hello_mapi, NULL)
    PHP_FE(print_profiles, NULL)
    PHP_FE(dump_profile, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry mapi_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_MAPI_EXTNAME,
    mapi_functions,
    NULL,//    PHP_MINIT(mapi),
    NULL, //PHP_MSHUTDOWN(mapi),
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_MAPI_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MAPI
ZEND_GET_MODULE(mapi)
#endif
/*
PHP_MINIT(mapi)
{
  int i = 0;

}

PHP_MSHUTDOWN(mapi)
{
  int i = 0;
}
*/

static char * mapiprofile_list(struct mapi_context *mapi_ctx, const char *profdb)
{
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	uint32_t		count = 0;

	memset(&proftable, 0, sizeof (struct SRowSet));
	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", retval);
		exit (1);
	}


	printf("We have %u profiles in the database:\n", proftable.cRows);



	for (count = 0; count != proftable.cRows; count++) {
		const char	*name = NULL;
		uint32_t	dflt = 0;

		name = proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		if (dflt) {
			printf("\tProfile = %s [default]\n", name);
		} else {
			printf("\tProfile = %s\n", name);
		}

	}
}




PHP_FUNCTION(print_profiles)
{
  struct mapi_context	*mapi_ctx;	/*!< mapi context */
  char *profdb = "/home/jag/.openchange/profiles.ldb";
  enum MAPISTATUS retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    mapi_errstr("MAPIInitialize", retval);
    RETURN_STRING("MAPI FAILED XXX", 1);
  }

  struct SRowSet proftable;
  uint32_t count = 0;

  memset(&proftable, 0, sizeof (struct SRowSet));

	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", retval);
		exit (1);
	}


	php_printf("We have %u profiles in the database:\n", proftable.cRows);

	for (count = 0; count != proftable.cRows; count++) {
		const char	*name = NULL;
		uint32_t	dflt = 0;

		name = proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		if (dflt) {
			php_printf("\tProfile = %s [default]\n", name);
		} else {
			php_printf("\tProfile = %s\n", name);
		}

	}
}

static void mapiprofile_dump(struct mapi_context *mapi_ctx, const char *profdb, const char *opt_profname)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_profile	*profile;
	char			*profname;
	char			*exchange_version = NULL;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_dump");
	profile = talloc(mem_ctx, struct mapi_profile);

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	talloc_free(profname);

	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		talloc_free(mem_ctx);
		mapi_errstr("OpenProfile", retval);
		exit (1);
	}

	switch (profile->exchange_version) {
	case 0x0:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2000");
		break;
	case 0x1:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2003/2007");
		break;
	case 0x2:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2010");
		break;
	default:
		printf("Error: unknown Exchange server\n");
		goto end;
	}

	printf("Profile: %s\n", profile->profname);
	printf("\texchange server == %s\n", exchange_version);
	printf("\tencryption      == %s\n", (profile->seal == true) ? "yes" : "no");
	printf("\tusername        == %s\n", profile->username);
	printf("\tpassword        == %s\n", profile->password);
	printf("\tmailbox         == %s\n", profile->mailbox);
	printf("\tworkstation     == %s\n", profile->workstation);
	printf("\tdomain          == %s\n", profile->domain);
	printf("\tserver          == %s\n", profile->server);

end:
	talloc_free(mem_ctx);
}

PHP_FUNCTION(dump_profile)
{
  // initialize and hardocde-values
        char* opt_profname  = NULL; // dumnp default by now
  struct mapi_context	*mapi_ctx;	/*!< mapi context */
  char *profdb = "/home/jag/.openchange/profiles.ldb";
  enum MAPISTATUS retval;
  retval = MAPIInitialize(&mapi_ctx, profdb);
  if (retval != MAPI_E_SUCCESS) {
    mapi_errstr("MAPIInitialize", retval);
    RETURN_STRING("MAPI FAILED XXX", 1);
  }
  // end

	TALLOC_CTX		*mem_ctx;


        //	enum MAPISTATUS		retval; //declarated in provisional section
	struct mapi_profile	*profile;
	char			*profname;
	char			*exchange_version = NULL;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_dump");
	profile = talloc(mem_ctx, struct mapi_profile);

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	talloc_free(profname);

	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		talloc_free(mem_ctx);
		mapi_errstr("OpenProfile", retval);
		exit (1);
	}

	switch (profile->exchange_version) {
	case 0x0:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2000");
		break;
	case 0x1:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2003/2007");
		break;
	case 0x2:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2010");
		break;
	default:
		php_printf("Error: unknown Exchange server\n");
		goto end;
	}

	php_printf("Profile: %s\n", profile->profname);
	php_printf("\texchange server == %s\n", exchange_version);
	php_printf("\tencryption      == %s\n", (profile->seal == true) ? "yes" : "no");
	php_printf("\tusername        == %s\n", profile->username);
	php_printf("\tpassword        == %s\n", profile->password);
	php_printf("\tmailbox         == %s\n", profile->mailbox);
	php_printf("\tworkstation     == %s\n", profile->workstation);
	php_printf("\tdomain          == %s\n", profile->domain);
	php_printf("\tserver          == %s\n", profile->server);

end:
	talloc_free(mem_ctx);


}



PHP_FUNCTION(hello_mapi)
{
	enum MAPISTATUS		retval;
  TALLOC_CTX		*mem_ctx;
  struct mapitest		mt;
   const char		*opt_profdb = NULL;
   // Initialize MAPI subsystem *\/ */
   if (!opt_profdb) {
     opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
   }



        retval = MAPIInitialize(&(mt.mapi_ctx), opt_profdb);
        if (retval != MAPI_E_SUCCESS) {
          mapi_errstr("MAPIInitialize", retval);
          RETURN_STRING("MAPI FAILED", 1);
        } else {
          RETURN_STRING("HELLO MaPI OK", 1);
        }

    RETURN_STRING("HELLO MPAI", 1);
}



