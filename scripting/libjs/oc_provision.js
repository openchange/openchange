/*
	backend code for provisioning an OpenChange server
	Copyright Andrew Tridgell 2005
	Copyright Julien Kerihuel 2005-2007
	Copyright Jerome Medegan  2006
	Released under the GNU GPL v3 or later

	Note to developers: This script is a light copy from Samba
	provisioning script with some modifications so it fit
	openchange needs. If the current file happens to be broken
	please report changes to <j.kerihuel@openchange.org> so we can
	update the code according to latest Samba modifications.
 */

sys = sys_init();

/*
  return current time as a nt time string
*/
function nttime()
{
	return "" + sys.nttime();
}

/*
  return current time as a ldap time string
*/
function ldaptime()
{
	return sys.ldaptime(sys.nttime());
}

/*
  return first part of hostname
*/
function hostname()
{
	var s = split(".", sys.hostname());
	return s[0];
}

/*
  erase an ldb, removing all records
*/
function ldb_erase(ldb)
{
	var res;

	/* delete the specials */
	ldb.del("@INDEXLIST");
	ldb.del("@ATTRIBUTES");
	ldb.del("@SUBCLASSES");
	ldb.del("@MODULES");
	ldb.del("@PARTITION");
	ldb.del("@KLUDGEACL");

	/* and the rest */
	attrs = new Array("dn");
     	var basedn = "";
     	var res = ldb.search("(&(|(objectclass=*)(dn=*))(!(dn=@BASEINFO)))", basedn, ldb.SCOPE_SUBTREE, attrs);
	var i;
	if (res.error != 0) {
		ldb_delete(ldb);
		return;
	}
	for (i=0;i<res.msgs.length;i++) {
		ldb.del(res.msgs[i].dn);
	}

     	var res = ldb.search("(&(|(objectclass=*)(dn=*))(!(dn=@BASEINFO)))", basedn, ldb.SCOPE_SUBTREE, attrs);
	if (res.error != 0 || res.msgs.length != 0) {
		ldb_delete(ldb);
		return;
	}
	assert(res.msgs.length == 0);
}

/*
  erase an ldb, removing all records
*/
function ldb_erase_partitions(info, ldb, ldapbackend)
{
	var rootDSE_attrs = new Array("namingContexts");
	var lp = loadparm_init();
	var j;

	var res = ldb.search("(objectClass=*)", "", ldb.SCOPE_BASE, rootDSE_attrs);
	assert(res.error == 0);
	assert(res.msgs.length == 1);
	if (typeof(res.msgs[0].namingContexts) == "undefined") {
		return;
	}	
	for (j=0; j<res.msgs[0].namingContexts.length; j++) {
		var anything = "(|(objectclass=*)(dn=*))";
		var attrs = new Array("dn");
		var basedn = res.msgs[0].namingContexts[j];
		var k;
		var previous_remaining = 1;
		var current_remaining = 0;

		if (ldapbackend && (basedn == info.subobj.DOMAINDN)) {
			/* Only delete objects that were created by provision */
			anything = "(objectcategory=*)";
		}

		for (k=0; k < 10 && (previous_remaining != current_remaining); k++) {
			/* and the rest */
			var res2 = ldb.search(anything, basedn, ldb.SCOPE_SUBTREE, attrs);
			var i;
			if (res2.error != 0) {
				info.message("ldb search failed: " + res.errstr + "\n");
				continue;
			}
			previous_remaining = current_remaining;
			current_remaining = res2.msgs.length;
			for (i=0;i<res2.msgs.length;i++) {
				ldb.del(res2.msgs[i].dn);
			}
			
			var res3 = ldb.search(anything, basedn, ldb.SCOPE_SUBTREE, attrs);
			if (res3.error != 0) {
				info.message("ldb search failed: " + res.errstr + "\n");
				continue;
			}
			if (res3.msgs.length != 0) {
				info.message("Failed to delete all records under " + basedn + ", " + res3.msgs.length + " records remaining\n");
			}
		}
	}
}

function open_ldb(info, dbname, erase)
{
	var ldb = ldb_init();
	ldb.session_info = info.session_info;
	ldb.credentials = info.credentials;
	ldb.filename = dbname;

	var connect_ok = ldb.connect(dbname);
	if (!connect_ok) {
		var lp = loadparm_init();
		sys.unlink(sprintf("%s/%s", lp.get("private dir"), dbname));
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

function setup_modify_ldif(ldif, info, ldb, failok)
{
	var lp = loadparm_init();
	var src = lp.get("setup directory") + "/" + ldif;

	var data = sys.file_load(src);
	data = substitute_var(data, info.subobj);

	var mod_res = ldb.modify(data);
	if (mod_res.error != 0) {
		info.message("ldb load failed: " + mod_res.errstr + "\n");
		if (!failok) {
			assert(mod_res.error == 0);
	        }
	}
	return (mod_res.error == 0);
}


function setup_ldb(ldif, info, dbname) 
{
	var erase = true;
	var failok = false;

	if (arguments.length >= 4) {
	        erase = arguments[3];
        }
	if (arguments.length == 5) {
	        failok = arguments[4];
        }
	var ldb = open_ldb(info, dbname, erase);
	if (setup_add_ldif(ldif, info, ldb, failok)) {
		var commit_ok = ldb.transaction_commit();
		if (!commit_ok) {
			info.message("ldb commit failed: " + ldb.errstring() + "\n");
			assert(commit_ok);
		}
	}
}

/*
  setup a ldb in the private dir
 */
function setup_ldb_modify(ldif, info, ldb)
{
	var lp = loadparm_init();

	var src = lp.get("setup directory") + "/" + ldif;

	var data = sys.file_load(src);
	data = substitute_var(data, info.subobj);

	var mod_res = ldb.modify(data);
	if (mod_res.error != 0) {
		info.message("ldb load failed: " + mod_res.errstr + "\n");
		return (mod_res.error == 0);
	}
	return (mod_res.error == 0);
}

/*
  setup a file in the private dir
 */
function setup_file(template, message, fname, subobj)
{
	var lp = loadparm_init();
	var f = fname;
	var src = lp.get("setup directory") + "/" + template;

	sys.unlink(f);

	var data = sys.file_load(src);
	data = substitute_var(data, subobj);

	ok = sys.file_save(f, data);
	if (!ok) {
		message("failed to create file: " + f + "\n");
		assert(ok);
	}
}

function provision_default_paths(subobj)
{
	var lp = loadparm_init();
	var paths = new Object();
	paths.shareconf = lp.get("private dir") + "/" + "share.ldb";
	paths.smbconf = lp.get("config file");
	paths.samdb = lp.get("sam database");
	return paths;
}

function provision_fix_subobj(subobj, message, paths)
{
	subobj.REALM       = strupper(subobj.REALM);
	subobj.HOSTNAME    = strlower(subobj.HOSTNAME);
	subobj.DOMAIN      = strupper(subobj.DOMAIN);
	assert(valid_netbios_name(subobj.DOMAIN));

	subobj.SAM_LDB		= paths.samdb;

	return true;
}

/*
 provision openchange - cautious this wipes all existing data!
*/
function provision(subobj, message, blank, paths, session_info, credentials)
{
	var lp = loadparm_init();
	var sys = sys_init();
	var info = new Object();

	var ok = provision_fix_subobj(subobj, message, paths);
	assert(ok);

	info.subobj = subobj;
	info.message = message;
	info.credentials = credentials;
	info.session_info = session_info;

	/* If no smb.conf has already been setup, then exit properly */
	var st = sys.stat(paths.smbconf);
	if (st == undefined) {
		message("You have to provision Samba4 server first");
		return false;
	}

	var samdb = open_ldb(info, paths.samdb, false);

	message("[OpenChange] Adding new Active Directory classes schemas\n");
	var add_ok = setup_add_ldif("oc_provision_schema.ldif", info, samdb, false);
	if (!add_ok){
		message("Failed to add Exchange classes schema to existing Active Directory\n");
		assert(add_ok);
	}

	message("[OpenChange] Extending existing Active Directory schemas\n");
	var modify_ok = setup_ldb_modify("oc_provision_schema_modify.ldif", info, samdb);
	if (!modify_ok) {
		message("Failed to extend existing Active Directory schemas");
		assert(modify_ok);
	}

	message("[OpenChange] Adding Configuration objects\n");
	add_ok = setup_add_ldif("oc_provision_configuration.ldif", info, samdb, false);
	if (!add_ok) {
		message("Failed to add configuration objects to existing Active Directory\n");
		assert(add_ok);
	}

	ok = samdb.transaction_commit();
	assert(ok);

}

/*
  guess reasonably default options for provisioning
*/
function provision_guess()
{
	var subobj = new Object();
	var nss = nss_init();
	var lp = loadparm_init();
	var rdn_list;
	random_init(local);

	subobj.REALM        = strupper(lp.get("realm"));
	subobj.DOMAIN       = lp.get("workgroup");
	subobj.HOSTNAME     = hostname();
	subobj.NETBIOSNAME  = strupper(subobj.HOSTNAME);

	assert(subobj.REALM);
	assert(subobj.DOMAIN);
	assert(subobj.HOSTNAME);
	
	subobj.VERSION      = version();
	subobj.DOMAINSID    = randsid();
	subobj.INVOCATIONID = randguid();
	subobj.KRBTGTPASS   = randpass(12);
	subobj.MACHINEPASS  = randpass(12);
	subobj.ADMINPASS    = randpass(12);
	subobj.DEFAULTSITE  = "Default-First-Site-Name";
	subobj.NEWGUID      = randguid;
	subobj.NTTIME       = nttime;
	subobj.LDAPTIME     = ldaptime;

	subobj.DNSDOMAIN    = strlower(subobj.REALM);
	subobj.DNSNAME      = sprintf("%s.%s", 
				      strlower(subobj.HOSTNAME), 
				      subobj.DNSDOMAIN);
	rdn_list = split(".", subobj.DNSDOMAIN);
	subobj.BASEDN       = "DC=" + join(",DC=", rdn_list);

	return subobj;
}

/*
  search for one attribute as a string
 */
function searchone(ldb, basedn, expression, attribute)
{
	var attrs = new Array(attribute);
	res = ldb.search(expression, basedn, ldb.SCOPE_SUBTREE, attrs);
	if (res.error != 0 ||
	    res.msgs.length != 1 ||
	    res.msgs[0][attribute] == undefined) {
		return undefined;
	}
	return res.msgs[0][attribute];
}


/*
  extend user record
*/
function oc_newuser(username, message, session_info, credentials)
{
	var lp		= loadparm_init();
	var samdb	= lp.get("sam database");
	var dnsdomain	= strlower(lp.get("realm"));
	var hostname	= hostname();
	var netbiosname = strupper(hostname);
	var ldb		= ldb_init();

	random_init(local);
	ldb.session_info = session_info;
	ldb.credentials = credentials;

	/* connect to the sam */
	var ok = ldb.connect(samdb);
	assert(ok);

	ldb.transaction_start();

	/* find the DNs for the domain and the domain users group */
	var attrs = new Array("defaultNamingContext");
	res = ldb.search("defaultNamingContext=*", "", ldb.SCOPE_BASE, attrs);
	assert(res.error == 0);
	assert(res.msgs.length == 1 && res.msgs[0].defaultNamingContext != undefined);
	var domain_dn = res.msgs[0].defaultNamingContext;
	assert(domain_dn != undefined);
	var dom_users = searchone(ldb, domain_dn, "name=Domain Users", "dn");
	assert(dom_users != undefined);

	var user_dn = sprintf("CN=%s,CN=Users,%s", username, domain_dn);
	var extended_user = sprintf("
dn: %s
changetype: modify
add: auxiliaryClass
displayName: %s
auxiliaryClass: msExchBaseClass
mailNickname: %s
homeMDB: CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=First Organization,CN=Microsoft Exchange,CN=Services,CN=Configuration,%s
legacyExchangeDN: /o=First Organization/ou=First Administrative Group/cn=Recipients/cn=%s
proxyAddresses: smtp:postmaster@%s
proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%s;
proxyAddresses: SMTP:%s@%s
", user_dn, username, username, netbiosname, netbiosname, domain_dn, username, dnsdomain, username, username, dnsdomain);

	message("Modify %s record\n", user_dn);
	ok = ldb.modify(extended_user);
	if (ok.error != 0) {
		message("Failed to modify %s - %s (%d)\n", user_dn, ldb.errstring(), ok.error);
		return false;
	}
	ldb.transaction_commit();
}

// Check whether a name is valid as a NetBIOS name.
// FIXME: There ae probably more constraints here
function valid_netbios_name(name)
{
	if (strlen(name) > 13) return false;
	if (strstr(name, ".")) return false;
	return true;
}

function provision_validate(subobj, message)
{
	return true;
}

return 0;