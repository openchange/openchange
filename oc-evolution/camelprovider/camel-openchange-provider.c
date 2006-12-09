/* camel-openchange-provider.c: openchange provider registration code */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <camel/camel-i18n.h>
#include <camel/camel-provider.h>
#include <camel/camel-session.h>
#include <camel/camel-url.h>

#include "camel-openchange-store.h"
#include "camel-openchange-transport.h"

static guint	openchange_url_hash (gconstpointer key);
static gint	openchange_url_equal (gconstpointer a, gconstpointer b);

CamelProviderConfEntry	openchange_conf_entries[] = {
  /* override the labels/defaults of the standard settings */
  { CAMEL_PROVIDER_CONF_LABEL, "username", NULL,
    /* i18n: the '_' should appear before the same letter it
       does in the evolution:mail-config.glade "User_name"
       translation (or not at all) */
    N_("Windows User_name:") },
  /* extra Exchange configuration settings */
  { CAMEL_PROVIDER_CONF_SECTION_START, "activedirectory", NULL,
    /* i18n: GAL is an Outlookism, AD is a Windowsism */
    N_("Global Address List / Active Directory") },
  { CAMEL_PROVIDER_CONF_ENTRY, "ad_server", NULL,
    /* i18n: "Global Catalog" is a Windowsism, but it's a
       technical term and may not have translations? */
    N_("_Global Catalog server name:") },
  { CAMEL_PROVIDER_CONF_CHECKSPIN, "ad_limit", NULL,
    N_("_Limit number of GAL responses: %s"), "y:1:500:10000" },
  { CAMEL_PROVIDER_CONF_SECTION_END },
  { CAMEL_PROVIDER_CONF_SECTION_START, "generals", NULL,
    N_("Options") },
  { CAMEL_PROVIDER_CONF_CHECKSPIN, "passwd_exp_warn_period", NULL,
    N_("_Password Expiry Warning period: %s"), "y:1:7:90" },
  { CAMEL_PROVIDER_CONF_CHECKBOX, "sync_offline", NULL,
    N_("Automatically synchroni_ze account locally"), "0" },
  { CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL,
    /* i18n: copy from evolution:camel-imap-provider.c */
    N_("_Apply filters to new messages in Inbox on this server"), "0" },
  { CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk", NULL,
    N_("Check new messages for _Junk contents"), "0" },
  { CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk_inbox", "filter_junk",
    N_("Only check for Junk messag_es in the Inbox folder"), "0" },
  { CAMEL_PROVIDER_CONF_HIDDEN, "auth-domain", NULL,
    NULL, "Openchange" },
  
  { CAMEL_PROVIDER_CONF_SECTION_END },
  { CAMEL_PROVIDER_CONF_END }
};

static CamelProvider	openchange_provider = {
  "openchange",
  N_("Openchange"),
  
  N_("For handling mail (and other data) on either Openchange or Microsoft Exchange servers"),
  
  "mail",
  
  CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
  CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_EXTERNAL,
  
  CAMEL_URL_NEED_USER | CAMEL_URL_HIDDEN_AUTH | CAMEL_URL_HIDDEN_HOST,
  
  openchange_conf_entries 
  
  /* ... */
};

CamelServiceAuthType	camel_openchange_ntlm_authtype = {

  N_("Secure Password"),

  N_("This option will connect to the Exchange server using "
     "secure password (NTLM) authentication."),
  "",
  TRUE
};

CamelServiceAuthType	camel_openchange_password_authtype = {
  N_("Plaintext Password"),
  N_("This option will connect to the Exchange server using "
     "standard plaintext password authentication."),
  "Basic",
  TRUE
};


void	camel_provider_module_init (void)
{
  openchange_provider.object_types[CAMEL_PROVIDER_STORE] = camel_openchange_store_get_type ();
  openchange_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = camel_openchange_transport_get_type ();
  //openchange_provider.authtypes = g_list_prepend (g_list_prepend (NULL, &camel_openchange_password_authtype), &camel_openchange_ntlm_authtype);
  openchange_provider.url_hash = openchange_url_hash;
  openchange_provider.url_equal = openchange_url_equal;
  
  bindtextdomain (GETTEXT_PACKAGE, CONNECTOR_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  openchange_provider.translation_domain = GETTEXT_PACKAGE;
  openchange_provider.translation_domain = GETTEXT_PACKAGE;
  
  camel_provider_register (&openchange_provider);
}

static const char	*openchange_username (const char *user)
{
  const char *p;
  
  if (user) {
    p = strpbrk (user, "\\/");
    if (p)
      return p + 1;
  }
  
  return user;
}

static guint		openchange_url_hash (gconstpointer key)
{
  const CamelURL *u = (CamelURL *)key;
  guint hash = 0;
  
  if (u->user)
    hash ^= g_str_hash (openchange_username (u->user));
  if (u->host)
    hash ^= g_str_hash (u->host);
  
  return hash;
}

static gboolean		check_equal (const char *s1, const char *s2)
{
  if (!s1)
    return s2 == NULL;
  else if (!s2)
    return FALSE;
  else
    return strcmp (s1, s2) == 0;
}

static gint		openchange_url_equal (gconstpointer a, gconstpointer b)
{
  const CamelURL *u1 = a, *u2 = b;
  
  return  check_equal (u1->protocol, u2->protocol) &&
    check_equal (openchange_username (u1->user),
		 openchange_username (u2->user)) &&
    check_equal (u1->host, u2->host);
}
