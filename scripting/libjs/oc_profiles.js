/*
	backend code for provisioning a MAPI Profile database
	Copyright Julien Kerihuel 2007
	Released under the GNU GPL v3 or later
*/

sys = sys_init();

function open_ldb(info, dbname, erase)
{
	var ldb = ldb_init();
	ldb.session_info = info.session_info;
	ldb.credentials = info.credentials;
	ldb.filename = dbname;

	var connect_ok = ldb.connect(dbname);
	if (!connect_ok) {
		var lp = loadparm_init();
		sys.unlink(sprintf("%s", dbname));
		connect_ok = ldb.connect(dbname);
		assert(connect_ok);
	}

	ldb.transaction_start();

	if (erase) {
		ldb_erase(ldb);	
	}
	return ldb;
}

/*
  setup a ldb in the private dir
 */
function setup_add_ldif(ldif, info, ldb, failok)
{
	var lp = loadparm_init();
	var src = lp.get("setup directory") + "/" + ldif;

	var data = sys.file_load(src);
	data = substitute_var(data, info.subobj);

	var add_res = ldb.add(data);
	if (add_res.error != 0) {
		info.message("ldb load failed: " + add_res.errstr + "\n");
		if (!failok) {
			assert(add_res.error == 0);
	        }
	}
	return (add_res.error == 0);
}

function provision_default_paths(subobj)
{
	var lp = loadparm_init();
	var paths = new Object();

	paths.smbconf = lp.get("config file");
	return paths;
}


function provision_guess()
{
	var subobj = new Object();
	var lp = loadparm_init();
	var rdn_list;

	return subobj;
}

/*
 provision MAPI profiles database - cautious this wipes all existing data!
*/
function provision(subobj, message, blank, paths, session_info, credentials)
{
	var lp = loadparm_init();
	var sys = sys_init();
	var info = new Object();

	info.subobj = subobj;
	info.message = message;
	info.credentials = credentials;
	info.session_info = session_info;
	
	var dbname = sprintf("%s/%s", subobj.PROFPATH, subobj.PROFDB);	

	var profiles = open_ldb(info, dbname, false);

	message("[OpenChange] Setting up " + dbname + " attributes\n");
	var ok = setup_add_ldif("oc_profiles_init.ldif", info, profiles, false);
	if (!ok) {
		message("Failed to add MAPI Profiles DomainDN\n");
		assert(ok);
	}

	message("[OpenChange] Adding MAPI Profiles database attributes\n");
	ok = setup_add_ldif("oc_profiles_schema.ldif", info, profiles, false);
	if (!ok) {
		message("Failed to add MAPI Profiles attributes\n");
		assert(ok);
	}

	ok = profiles.transaction_commit();
	if (!ok) {
		assert(ok);
	}
	return true;
}

function provision_validate(subobj, message)
{
	var lp = loadparm_init();

	if (subobj.PROFPATH == undefined) {
		message("Invalid Profile database path\n");
		return false;
	}

	if (subobj.PROFDB == undefined) {
		message("Invalid Profile database name\n");
		return false;
	}

	return true;
}

return 0;