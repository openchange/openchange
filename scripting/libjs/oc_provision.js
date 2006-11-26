/*
	backend code for provisioning an OpenChange server
	Copyright Andrew Tridgell 2005
	Copyright Julien Kerihuel 2005-2006
	Copyright Jerome Medegan  2006
	Released under the GNU GPL v2 or later
*/

/* used to generate sequence numbers for records */
provision_next_usn = 1;

sys = sys_init();

/*
  return current time as a nt time string
*/
function nttime()
{
	return "" + sys.nttime();
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
  return current time as a ldap time string
*/
function ldaptime()
{
	return sys.ldaptime(sys.nttime());
}

/*
  erase an ldb, removing all records
*/
function ldb_erase(ldb)
{
	var attrs = new Array("dn");

	/* delete the specials */
	ldb.del("@INDEXLIST");
	ldb.del("@ATTRIBUTES");
	ldb.del("@SUBCLASSES");
	ldb.del("@MODULES");

	/* and the rest */
	var res = ldb.search("(|(objectclass=*)(dn=*))", attrs);
	var i;
	if (typeof(res) == "undefined") {
		ldb_delete(ldb);
		return;
	}
	for (i=0;i<res.length;i++) {
		ldb.del(res[i].dn);
	}
	res = ldb.search("(objectclass=*)", attrs);
	if (typeof(res) != "undefined") {
		if (res.length != 0) {
			ldb_delete(ldb);
			return;
		}
		assert(res.length == 0);
	}
}

/*
  add new records to an existing ldb in the private dir
 */
function update_ldb(ldif, dbname, subobj, credentials, session_info, operation)
{
	var ldb = ldb_init();
	var lp = loadparm_init();
	ldb.session_info = session_info;
	ldb.credentials = credentials;

	var src = lp.get("setup directory") + "/" + ldif;
	var data = sys.file_load(src);
	data = substitute_var(data, subobj);

	message("fetching info from %s\n", src);
	message("opening dbname: %s\n", dbname);
	message("%s\n", data);
	ldb.filename = dbname;

	var connect_ok = ldb.connect(dbname);
	assert(connect_ok);

	ldb.transaction_start();

	if (operation) {
		message("Modification\n");
		var ok = ldb.modify(data);
	} else {
		message("Add\n");
		var ok = ldb.add(data);
	}
	if (ok != true) {
		message("ldb commit failed: " + ldb.errstring() + "\n");
	}
	assert(ok);

	ldb.transaction_commit();
}


/*
  setup a ldb in the private dir
 */
function setup_ldb(ldif, dbname, subobj)
{
	var erase = true;
	var extra = "";
	var ldb = ldb_init();
	var lp = loadparm_init();

	if (arguments.length >= 4) {
		extra = arguments[3];
	}

	if (arguments.length == 5) {
	        erase = arguments[4];
        }

	var src = lp.get("setup directory") + "/" + ldif;
	var data = sys.file_load(src);
	data = data + extra;
	data = substitute_var(data, subobj);

	ldb.filename = dbname;

	var connect_ok = ldb.connect(dbname);
	assert(connect_ok);

	if (erase) {
		ldb_erase(ldb);	
	}

	var add_ok = ldb.add(data);
	assert(add_ok);
}

function provision_default_paths(subobj)
{
	var lp = loadparm_init();
	var paths = new Object();
	paths.smbconf = lp.get("config file");
	paths.store = "store.ldb";
	paths.samdb = "sam.ldb";
	return paths;
}

function provision_schema(subobj, message, blank, paths, creds, system_session)
{
	var data = "";
	var lp = loadparm_init();
	var sys = sys_init();

	/*
	 some options need to be upper/lower case
	*/
	subobj.DOMAIN       = strupper(subobj.DOMAIN);
	subobj.REALM	    = strupper(subobj.REALM);
	assert(valid_netbios_name(subobj.DOMAIN));

	provision_next_usn = 1;

	message("[OpenChange] Adding new Active Directory attributes schemas\n");

	message("[OpenChange] Adding new Active Directory classes schemas\n");
	update_ldb("oc_provision_schema.ldif", paths.samdb, subobj, creds, system_session, "");	
	message("[OpenChange] Extending existing Active Directory Schemas\n");
	update_ldb("oc_provision_schema_modify.ldif", paths.samdb, subobj, creds, system_session, "modify");
}

/*
  provision openchange - caution, this wipes all existing data!
*/
function provision(subobj, message, blank, paths, creds, system_session)
{
	var data = "";
	var lp = loadparm_init();
	var sys = sys_init();
	
	/*
	  some options need to be upper/lower case
	*/
	subobj.DOMAIN       = strupper(subobj.DOMAIN);
	subobj.REALM	    = strupper(subobj.REALM);
	assert(valid_netbios_name(subobj.DOMAIN));

	provision_next_usn = 1;

	message("[OpenChange] Adding Configuration objects\n");
	update_ldb("oc_provision_configuration.ldif", paths.samdb, subobj, creds, system_session, "");
}

/*
  guess reasonably default options for provisioning
*/
function provision_guess()
{
	var subobj = new Object();
	var lp = loadparm_init();
	random_init(local);

	subobj.HOSTNAME	    = hostname();
	subobj.NETBIOSNAME  = strupper(subobj.HOSTNAME);
	subobj.DOMAIN 	    = lp.get("workgroup");
	subobj.REALM 	    = lp.get("realm");
	assert(subobj.DOMAIN);
	assert(subobj.REALM);
	subobj.NTTIME       = nttime;
	subobj.LDAPTIME     = ldaptime;
	subobj.DOMAINGUID   = randguid();
	subobj.DOMAINSID    = randsid();
	subobj.DNSDOMAIN    = strlower(subobj.REALM);
	subobj.DNSNAME      = sprintf("%s.%s", 
				      strlower(subobj.HOSTNAME), 
				      subobj.DNSDOMAIN);
	var rdn_list = split(".", subobj.DNSDOMAIN);
	subobj.BASEDN       = "DC=" + join(",DC=", rdn_list);
	var rdns = split(",", subobj.BASEDN);
	subobj.RDN_DC = substr(rdns[0], strlen("DC="));

	return subobj;
}

/*
  search for one attribute as a string
 */
function searchone(ldb, expression, attribute)
{
	var attrs = new Array(attribute);
	res = ldb.search(expression, attrs);
	if (res.length != 1 ||
	    res[0][attribute] == undefined) {
		return undefined;
	}
	return res[0][attribute];
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
	var domain_dn = searchone(ldb, "objectClass=domainDNS", "dn");
	assert(domain_dn != undefined);
	var dom_users = searchone(ldb, "name=Domain Users", "dn");
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
	if (ok != true) {
		message("Failed to modify %s - %s\n", user_dn, ldb.errstring());
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
