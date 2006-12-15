/* 
   OpenChange NSPI implementation

   Check supported MAPI Property Tags for each Nspi functions

   Copyright (C) Pauline Khun 2006
   Copyright (C) Julien Kerihuel 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/**  Nspi functions Torture test  **/

/**  Torture Test  (Second version)
 ** The purpose of this new torture test is to filter the property tags which
 ** function or not with the function choosed. The test first creates a new
 ** directory tree organized by the exchange server version tested, the Nspi
 ** functions and the property tags (failing or succeeding).
 ** Each file (scenario) then represents a property tag behavior according to
 ** the Exchange version and the function tested, and contains an test output
 ** formatted in XML.
 */

/**  How to
 ** 1 - Set the smb.conf file :
 **     - Add to the globals section :
 **       PR_TAGS:tags_conf_path = [your_path]/openchange/trunk/source/torture/conf/nspi_scantags.conf
 **       PR_TAGS:tags_path = [your_path]/openchange/trunk/source/torture/conf/proptags.conf
 **
 ** 2 - Change the nspi_scantags.conf file :
 **     - Set the scenarios_path to the path you've choosen to nest the
 **       scenarios.
 **     - Set the function to test : function = [GetMatches | GetProps | QueryRows]
 **
 ** 3 - Torture test name : RPC-NSPI_SCANTAGS
 **
 */

#include "openchange.h"
#include <torture/torture.h>
#include "torture_proto.h"
#include "libmapi/include/nspi.h"
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/proto.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/mapi_proto.h"

#define FAILED			"Failed"
#define SUCCEEDED		"Succeeded"
#define PARENT			".."
#define XMLPATTERNBEGIN		"<exchange version='%s'>\n\t<function name='Nspi_%s'>\n\t\t<tag name='%s' value='0x%x'>"
#define XMLPATTERNEND		"\n\t\t</tag>\n\t</function>\n</exchange>\n"

/* Global vars */
FILE	*stream;

BOOL get_special_props(struct nspi_context *);

void torture_leave_domain(void *);

struct tagsScenarios
{
	const char	*server;
	const char	*path;
	const char	*nspiFct;
};

struct NspiFunction
{
	const char	*name;
	BOOL		(*fctPointer)(struct nspi_context *nspi, struct SPropTagArray *SPropTagArray);
	BOOL		(*fctAux)(struct nspi_context *nspi);
};

BOOL nspi_QueryRows(struct nspi_context *, struct SPropTagArray *);
BOOL nspi_GetMatches(struct nspi_context *, struct SPropTagArray *);
BOOL nspi_GetProps(struct nspi_context *, struct SPropTagArray *);

struct NspiFunction	gl_NspiFunctions[] =
{
	{"QueryRows", &nspi_QueryRows, &get_special_props},
	{"GetMatches", &nspi_GetMatches, NULL},
	{"GetProps", &nspi_GetProps, &get_special_props},
	{0, 0}
};

static void ndr_print_scantags_helper(struct ndr_print *ndr, const char *format, ...) _PRINTF_ATTRIBUTE(2,3)
{
        va_list ap;
        char *s = NULL;
        int i;

        va_start(ap, format);
        vasprintf(&s, format, ap);
        va_end(ap);

        for (i=0;i<ndr->depth;i++) {
                fprintf(stream, "    ");
        }

	fprintf(stream, "%s\n", s);
        free(s);
}

BOOL	get_special_props(struct nspi_context *nspi)
{
	BOOL				ret = True;
	struct SPropTagArray		*SPropTagArray;
	
	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 2, PR_INSTANCE_KEY, PR_EMAIL_ADDRESS);
	ret &= nspi_GetMatches(nspi, SPropTagArray);
	return (ret);
}

static BOOL do_nspi_function(const char *nspiFunction, struct nspi_context *nspi, struct SPropTagArray *SPropTagArray)
{
	int	i;
	
	for (i = 0; gl_NspiFunctions[i].name; i++)
		if (!strcmp(nspiFunction, gl_NspiFunctions[i].name))
			return (gl_NspiFunctions[i].fctPointer(nspi, SPropTagArray));
	DEBUG(0, ("%s: Not a Nspi funtion", nspiFunction));
	exit(EXIT_FAILURE);
}

static void chdir_scenarios(const char *path)
{
	if (chdir(path) < 0)
	{
		DEBUG(0, ("%s : Scenarios setup failed\n", path));
		exit(EXIT_FAILURE);
	}
}

static BOOL start_process(const char *section, void *other)
{
	DEBUG(0, ("-------------------------------\n- Scenarios creation -\n"));
	return (True);
}

static BOOL init_scenarios(const char *param, const char *value, void *other)
{
	struct tagsScenarios	*scenarios;
	
	scenarios = (struct tagsScenarios *)other;
	if (!strcmp(param, "server_version")) {
		scenarios->server = talloc_strdup(scenarios, value);
		DEBUG(0, ("[Server] :\t\t%s\n", scenarios->server));
	}
	else if (!strcmp(param, "scenarios_path")) {
		scenarios->path = talloc_strdup(scenarios, value);
		DEBUG(0, ("[Scenarios path] :\t%s\n", scenarios->path));
	}
	else if (!strcmp(param, "function")) {
		scenarios->nspiFct = talloc_strdup(scenarios, value);
		DEBUG(0, ("[Nspi Function] :\t%s\n", scenarios->nspiFct));
	}
	else
		return (False);
	return (True);
}

static BOOL setup_directory(const char *path)
{
	DIR	*dir;
	
	if ((dir = opendir(path)) == NULL)
	{
		DEBUG(0, ("%s : Not found\nMake directory...\n", path));
		if (mkdir(path, 0700) < 0)
		{
			DEBUG(0, ("%s : Directory creation failed", path));
			exit(EXIT_FAILURE);
		}
		DEBUG(0, ("%s : Creation succeed\n", path));
	}
	else
		DEBUG(0, ("%s : Already exists\n", path));
	return (True);
}

static struct tagsScenarios *setupScenarios(TALLOC_CTX *mem_ctx)
{
	struct tagsScenarios   	*scenarios;
	const char	       	*conf_path =
		(const char *)lp_parm_string(-1, "PR_TAGS", "tags_conf_path");
	
	scenarios = talloc(mem_ctx, struct tagsScenarios);
	if (pm_process(conf_path, start_process, init_scenarios, scenarios) == False)
	{
		DEBUG(0, ("pm_process: %s: Failed\n", conf_path));
		exit(EXIT_FAILURE);
	}
	setup_directory(scenarios->path);
	chdir_scenarios(scenarios->path);
	setup_directory(scenarios->server);
	chdir_scenarios(scenarios->server);
	setup_directory(scenarios->nspiFct);
	chdir_scenarios(scenarios->nspiFct);
	setup_directory("Failed");
	setup_directory("Succeeded");
  return (scenarios);
}

BOOL	torture_rpc_scantags(struct torture_context *torture)
{
	uint32_t	       	proptag;
	BOOL		       	ret = True;
	NTSTATUS	       	status;
	TALLOC_CTX	       	*mem_ctx;
	struct dcerpc_pipe     	*p;
	struct nspi_context    	*nspi;
	struct SPropTagArray   	*SPropTagArray;
	struct tagsScenarios   	*scenarios;
	const char	       	*tags_path = lp_parm_string(-1, "PR_TAGS", "tags_path");
	const char	       	*binding = lp_parm_string(-1, "torture", "binding");
	struct cli_credentials	*nspi_credentials;
	struct test_join       	*user_ctx = (struct test_join *) NULL;
	char		       	*user_password = (char *) NULL;
	const char	       	*userdomain;
	
	mem_ctx = talloc_init("torture_rpc_scantags");
	
	/* We add the user in the AD () */
	userdomain = lp_parm_string(-1, "torture", "userdomain");
	if (!userdomain) {
		userdomain = lp_workgroup();
	}

#if 0 /* FIXME */
	user_ctx = torture_create_testuser(TEST_USER_NAME,
					   userdomain,
					   ACB_NORMAL, 
					   (const char **)&user_password);
#endif
	if (!user_ctx) {
		printf("Failed to create the user\n");
		return False;
	}

	/* We extend the user with Exchange attributes */
#if 0 /* FIXME */
	status = torture_create_exchangeuser(mem_ctx, torture_join_user_sid(user_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		torture_leave_domain(user_ctx);
		talloc_free(mem_ctx);
		return False;
	}
#endif

	/* We now setup and play the scenario */
	nspi_credentials = cli_credentials_init(mem_ctx);
	cli_credentials_set_conf(nspi_credentials);
	cli_credentials_set_workstation(nspi_credentials, TEST_MACHINE_NAME, CRED_SPECIFIED);
	cli_credentials_set_domain(nspi_credentials, userdomain, CRED_SPECIFIED);
	cli_credentials_set_username(nspi_credentials, TEST_USER_NAME, CRED_SPECIFIED);
	cli_credentials_set_password(nspi_credentials, user_password, CRED_SPECIFIED);

	status = dcerpc_pipe_connect(mem_ctx, &p, binding,
				     &dcerpc_table_exchange_nsp,
				     nspi_credentials, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return False;
	}

	scenarios = setupScenarios(mem_ctx);
	
	nspi = nspi_bind(mem_ctx, p, nspi_credentials);
	if (!nspi) {
		torture_leave_domain(user_ctx);
		return False;
	}
	nspi->mem_ctx = mem_ctx;
	
	ret &= nspi_GetHierarchyInfo(nspi);

	if (strcmp(scenarios->nspiFct, "GetMatches"))
		get_special_props(nspi);
	
	
	while ((SPropTagArray = set_SPropTagFile(mem_ctx, tags_path, "proptags")))
	{
		const char	*tagname;
		
		ret &= do_nspi_function(scenarios->nspiFct, nspi, SPropTagArray);
		if (nspi->rowSet->cRows)
		{
			proptag = nspi->rowSet->aRow[0].lpProps[0].ulPropTag;
			tagname = get_proptag_name(proptag);
			if (tagname)
			{
				if ((proptag & 0xFFFF) == PT_ERROR) {
					chdir_scenarios(FAILED);
				} else {
					chdir_scenarios(SUCCEEDED);
				}
				stream = fopen(tagname, "w");
				if (stream != NULL)
				{
					struct ndr_print *ndr_print;

					ndr_print = talloc_zero(mem_ctx, struct ndr_print);
					ndr_print->print = ndr_print_scantags_helper;
					ndr_print->depth = 1;
					

					fprintf(stream, XMLPATTERNBEGIN, scenarios->server, scenarios->nspiFct, tagname, proptag);
					ndr_print_SRow(ndr_print, tagname, &nspi->rowSet->aRow[0]);
					fprintf(stream, XMLPATTERNEND);
					fclose(stream);
				}
				chdir_scenarios(PARENT);
				if (tagname && ((proptag & 0xFFFF) != 0xa))
				DEBUG(0, ("%s\n", tagname));
			}
			else {
				printf("proptag = 0x%.8x doesn't exist\n", proptag);
			}
		}
	}

	/* We clean up the AD: remove test user */
	DEBUG(0, ("Cleaning up the AD."));
#if 0  /* FIXME */
	torture_leave_domain(user_ctx);
#endif

	talloc_free(mem_ctx);
	return (True);
}
