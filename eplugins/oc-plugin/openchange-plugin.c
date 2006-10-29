#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkselection.h>

#include <e-util/e-plugin.h>
#include <e-util/e-config.h>
#include <e-util/e-account.h>


#include <libintl.h>
//#include <camel/camel-i18n.h>
#include <camel/camel-provider.h>
#include <camel/camel-url.h>
#include <camel/camel-service.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>

#include <libedataserver/e-xml-hash-utils.h>


int		e_plugin_lib_enable(EPluginLib *ep, int enable);
GtkWidget	*org_gnome_openchange_check_options(EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget	*org_gnome_openchange_account_settings(EPlugin *epl, EConfigHookItemFactoryData *data);
void		org_gnome_openchange_url(EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget	*org_gnome_openchange_account_settings(EPlugin *epl, EConfigHookItemFactoryData *data);

CamelProviderConfEntry	openchange_conf_entries[] = {
  { CAMEL_PROVIDER_CONF_LABEL, "username", NULL, "Openchange User_name:"},
  
  /* extra Openchange configuration settings */
  { CAMEL_PROVIDER_CONF_SECTION_START, "activedirectory", NULL,
    /* i18n: GAL is an Outlookism, AD is a Windowsism */
    "Global Address List / Active Directory" },
  
  { CAMEL_PROVIDER_CONF_SECTION_END },
  { CAMEL_PROVIDER_CONF_END }
};

static CamelProvider openchange_provider = {
  "openchange",
  "Openchange",
  "For handling mail and other information on Openchange Server",
  "mail",
  CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE,
  CAMEL_URL_NEED_USER | CAMEL_URL_HIDDEN_AUTH | CAMEL_URL_HIDDEN_HOST,
  openchange_conf_entries
};




CamelType	camel_openchange_store_get_type(){
  
  return (0);
}

CamelType	camel_openchange_transport_get_type(){

  return (0);
}


void camel_provider_module_init(void)
{
  openchange_provider.object_types[CAMEL_PROVIDER_STORE] = camel_openchange_store_get_type();
  openchange_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = camel_openchange_transport_get_type();

  bindtextdomain("Openchange", NULL);
  bind_textdomain_codeset("Openchange", "UTF-8");

  camel_provider_register(&openchange_provider);
}



int	e_plugin_lib_enable(EPluginLib *ep, int enable)
{
  if (enable) {
    printf("Openchange plugin enabled\n");

  } else {
    printf("Openchange plugin disabled\n");
  }
 return 0;
}


GtkWidget	*org_gnome_openchange_account_settings(EPlugin *epl, EConfigHookItemFactoryData *data)
{
  return (0);
}

GtkWidget	*org_gnome_openchange_check_options(EPlugin *epl, EConfigHookItemFactoryData *data)
{
  return (0);
}


