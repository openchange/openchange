ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = 	$(LIBDOVECOT_INCLUDE) 		\
		$(LIBDOVECOT_STORAGE_INCLUDE) 	\
		$(LIBDOVECOT_IMAP_INCLUDE) 	\
		$(MAPISTORE_CFLAGS)		\
		$(NANOMSG_CFLAGS)

LIBS += $(MAPISTORE_LIBS) $(NANOMSG_LIBS)

openchange_moduledir = $(dovecot_moduledir)
openchange_module_LTLIBRARIES = lib90_openchange_plugin.la

lib90_openchange_plugin_la_LDFLAGS = -module -avoid-version -shared
lib90_openchange_plugin_la_SOURCES = openchange-plugin.c
