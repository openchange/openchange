# Makefile for OpenChange
# Written by Jelmer Vernooij <jelmer@openchange.org>, 2005.

default: all

# Until we add proper dependencies for all the C files:
.NOTPARALLEL:

config.mk: config.status config.mk.in
	./config.status

config.status: configure
	./configure

configure: configure.ac
	./autogen.sh

samba:
	./script/installsamba4.sh all

samba-git: 
	./script/installsamba4.sh git-all

ifneq ($(MAKECMDGOALS), samba)
ifneq ($(MAKECMDGOALS), samba-git)
include config.mk
endif
endif

#################################################################
# top level compilation rules
#################################################################

all: 		$(OC_IDL)		\
		$(OC_LIBS)		\
		$(OC_TOOLS)		\
		$(OC_TORTURE)		\
		$(OC_SERVER)		\
		$(SWIGDIRS-ALL)		\
		$(PYMAPIALL)		\
		$(COVERAGE_INIT)

install: 	all 			\
		installlib 		\
		installpc 		\
		installheader 		\
		$(OC_TOOLS_INSTALL) 	\
		$(OC_SERVER_INSTALL) 	\
		$(OC_TORTURE_INSTALL) 	\
		$(SWIGDIRS-INSTALL) 	\
		$(PYMAPIINSTALL) \
		installnagios

installlib:	$(OC_LIBS_INSTALL)
installpc:	$(OC_LIBS_INSTALLPC)
installheader:	$(OC_LIBS_INSTALLHEADERS)

uninstall:: 	$(OC_LIBS_UNINSTALL) 	\
		$(OC_TOOLS_UNINSTALL) 	\
		$(OC_SERVER_UNINSTALL) 	\
		$(OC_TORTURE_UNINSTALL) \
		$(SWIGDIRS-UNINSTALL) \
		$(PYMAPIUNINSTALL)

dist:: distclean
	./autogen.sh

distclean:: clean
	rm -rf autom4te.cache
	rm -f Doxyfile
	rm -f libmapi/Doxyfile
	rm -f libmapiadmin/Doxyfile
	rm -f libocpf/Doxyfile
	rm -f libmapi++/Doxyfile
	rm -f mapiproxy/Doxyfile
	rm -f config.status config.log
	rm -f stamp-h1
	rm -f utils/mapitest/Doxyfile
	rm -f intltool-extract intltool-merge intltool-update
	rm -rf apidocs
	rm -rf gen_ndr
	rm -f tags

clean::
	rm -f *~
	rm -f */*~
	rm -f */*/*~
	rm -f doc/examples/mapi_sample1
	rm -f doc/examples/fetchappointment
	rm -f doc/examples/fetchmail

re:: clean install

#################################################################
# Suffixes compilation rules
#################################################################

.SUFFIXES: .c .o .h .po .idl

.idl.h:
	@echo "Generating $@"
	@$(PIDL) --outputdir=gen_ndr --header -- $<

.c.o:
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

.c.po:
	@echo "Compiling $< with -fPIC"
	@$(CC) $(CFLAGS) -fPIC -c $< -o $@



#################################################################
# IDL compilation rules
#################################################################

idl: gen_ndr gen_ndr/ndr_exchange.h gen_ndr/ndr_property.h

exchange.idl: mapitags_enum.h mapicodes_enum.h

gen_ndr:
	@echo "Creating the gen_ndr directory"
	mkdir -p gen_ndr

gen_ndr/ndr_%.h gen_ndr/ndr_%.c: %.idl %.h
	@echo "Generating $@"
	@$(PIDL) --outputdir=gen_ndr --ndr-parser -- $<

gen_ndr/ndr_%_c.h gen_ndr/ndr_%_c.c: %.idl %.h
	@echo "Generating $@"
	@$(PIDL) --outputdir=gen_ndr --client -- $<

gen_ndr/ndr_%_s.c: %.idl
	@echo "Generating $@"
	@$(PIDL) --outputdir=gen_ndr --server -- $<



#################################################################
# libmapi compilation rules
#################################################################

LIBMAPI_SO_VERSION = 0

libmapi:	idl					\
		libmapi/version.h			\
		libmapi/proto.h				\
		libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)	

libmapi-install:	libmapi			\
			libmapi-installpc	\
			libmapi-installlib	\
			libmapi-installheader	\
			libmapi-installscript

libmapi-uninstall:	libmapi-uninstallpc	\
			libmapi-uninstalllib	\
			libmapi-uninstallheader	\
			libmapi-uninstallscript

libmapi-clean::
	rm -f libmapi/*.o libmapi/*.po
	rm -f libmapi/*.gcno libmapi/*.gcda libmapi/*.gcov
	rm -f libmapi/tests/*.o libmapi/tests/*.po
	rm -f libmapi/tests/*.gcno libmapi/tests/*.gcda
	rm -f libmapi/socket/*.o libmapi/socket/*.po
	rm -f libmapi/socket/*.gcno libmapi/socket/*.gcda
	rm -f libmapi/util/*.o libmapi/util/*.po
	rm -f libmapi/util/*.gcno libmapi/util/*.gcda
	rm -f libmapi/version.h
ifneq ($(SNAPSHOT), no)
	rm -f libmapi/utf8_convert.yy.c
	rm -f libmapi/mapicode.c libmapi/mapicode.h
	rm -f libmapi/mapitags.c libmapi/mapitags.h
	rm -f libmapi/mapi_nameid.h libmapi/mapi_nameid_private.h
	rm -f libmapi/proto.h
	rm -f libmapi/proto_private.h
	rm -f mapicodes_enum.h
	rm -f mapitags_enum.h
endif
	rm -f gen_ndr/ndr_exchange*
	rm -f gen_ndr/exchange.h
	rm -f gen_ndr/ndr_property*
	rm -f gen_ndr/property.h
	rm -f ndr_mapi.o ndr_mapi.po
	rm -f ndr_mapi.gcno ndr_mapi.gcda
	rm -f *~
	rm -f */*~
	rm -f */*/*~
	rm -f libmapi.$(SHLIBEXT).$(PACKAGE_VERSION) libmapi.$(SHLIBEXT).$(LIBMAPI_SO_VERSION) \
		  libmapi.$(SHLIBEXT)

clean:: libmapi-clean

libmapi-distclean::
	rm -f libmapi.pc

distclean:: libmapi-distclean

libmapi-installpc:
	@echo "[*] install: libmapi pc files"
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 0644 libmapi.pc $(DESTDIR)$(libdir)/pkgconfig

libmapi-installlib:
	@echo "[*] install: libmapi library"
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 0755 libmapi.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libmapi.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapi.$(SHLIBEXT)
ifeq ($(MANUALLY_CREATE_SYMLINKS), yes)
	ln -sf libmapi.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapi.$(SHLIBEXT).$(LIBMAPI_SO_VERSION)
endif

libmapi-installheader:
	@echo "[*] install: libmapi headers"
	$(INSTALL) -d $(DESTDIR)$(includedir)/libmapi 
	$(INSTALL) -d $(DESTDIR)$(includedir)/libmapi/socket 
	$(INSTALL) -d $(DESTDIR)$(includedir)/gen_ndr
	$(INSTALL) -m 0644 libmapi/dlinklist.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/libmapi.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/proto.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/nspi.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/emsmdb.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_ctx.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_provider.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_id_array.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_notification.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_object.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_profile.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapi_nameid.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapidefs.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/version.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/mapicode.h $(DESTDIR)$(includedir)/libmapi/
	$(INSTALL) -m 0644 libmapi/socket/netif.h $(DESTDIR)$(includedir)/libmapi/socket/
	$(INSTALL) -m 0644 gen_ndr/exchange.h $(DESTDIR)$(includedir)/gen_ndr/
	$(INSTALL) -m 0644 gen_ndr/property.h $(DESTDIR)$(includedir)/gen_ndr/

libmapi-installscript:
	$(INSTALL) -d $(DESTDIR)$(datadir)/setup/profiles
	$(INSTALL) -m 0644 setup/profiles/oc_profiles* $(DESTDIR)$(datadir)/setup/profiles/

libmapi-uninstallpc:
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libmapi.pc

libmapi-uninstalllib:
	rm -f $(DESTDIR)$(libdir)/libmapi.*

libmapi-uninstallheader:
	rm -rf $(DESTDIR)$(includedir)/libmapi
	rm -f $(DESTDIR)$(includedir)/gen_ndr/exchange.h
	rm -f $(DESTDIR)$(includedir)/gen_ndr/property.h

libmapi-uninstallscript:
	rm -f $(DESTDIR)$(datadir)/setup/profiles/oc_profiles*
	rm -rf $(DESTDIR)$(datadir)/setup/profiles

libmapi.$(SHLIBEXT).$(PACKAGE_VERSION): 		\
	libmapi/IABContainer.po				\
	libmapi/IProfAdmin.po				\
	libmapi/IMAPIContainer.po			\
	libmapi/IMAPIFolder.po				\
	libmapi/IMAPIProp.po				\
	libmapi/IMAPISession.po				\
	libmapi/IMAPISupport.po				\
	libmapi/IStream.po				\
	libmapi/IMAPITable.po				\
	libmapi/IMessage.po				\
	libmapi/IMsgStore.po				\
	libmapi/IStoreFolder.po				\
	libmapi/IUnknown.po				\
	libmapi/IMSProvider.po				\
	libmapi/IXPLogon.po				\
	libmapi/FXICS.po				\
	libmapi/utils.po 				\
	libmapi/property.po				\
	libmapi/cdo_mapi.po 				\
	libmapi/lzfu.po					\
	libmapi/mapi_object.po				\
	libmapi/mapi_id_array.po			\
	libmapi/mapitags.po				\
	libmapi/mapidump.po				\
	libmapi/mapicode.po 				\
	libmapi/mapi_nameid.po				\
	libmapi/emsmdb.po				\
	libmapi/nspi.po 				\
	libmapi/simple_mapi.po				\
	libmapi/util/lcid.po 				\
	libmapi/util/codepage.po			\
	libmapi/freebusy.po				\
	libmapi/x500.po 				\
	ndr_mapi.po					\
	gen_ndr/ndr_exchange.po				\
	gen_ndr/ndr_exchange_c.po			\
	gen_ndr/ndr_property.po				\
	libmapi/socket/interface.po			\
	libmapi/socket/netif.po				\
	libmapi/utf8_convert.yy.po
	@echo "Linking $@"
	@$(CC) $(DSOOPT) $(CFLAGS) $(LDFLAGS) -Wl,-soname,libmapi.$(SHLIBEXT).$(LIBMAPI_SO_VERSION) -o $@ $^ $(LIBS)


libmapi.$(SHLIBEXT).$(LIBMAPI_SO_VERSION): libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	ln -fs $< $@

libmapi/version.h: VERSION
	@./script/mkversion.sh 	VERSION libmapi/version.h $(PACKAGE_VERSION) $(top_builddir)/

libmapi/utf8_convert.yy.c: 	libmapi/utf8_convert.l
	@echo "Generating $@"
	@$(FLEX) -Plibmapi_utf8_convert_ -t $< > $@

# Avoid warnings:
libmapi/utf8_convert.yy.o: CFLAGS=
libmapi/utf8_convert.yy.po: CFLAGS=

libmapi/proto.h libmapi/proto_private.h:		\
	libmapi/nspi.c					\
	libmapi/emsmdb.c				\
	libmapi/cdo_mapi.c				\
	libmapi/simple_mapi.c				\
	libmapi/mapitags.c				\
	libmapi/mapicode.c				\
	libmapi/mapidump.c				\
	libmapi/mapi_object.c				\
	libmapi/mapi_id_array.c				\
	libmapi/mapi_nameid.c				\
	libmapi/property.c				\
	libmapi/IABContainer.c				\
	libmapi/IProfAdmin.c				\
	libmapi/IMAPIContainer.c			\
	libmapi/IMAPIFolder.c 				\
	libmapi/IMAPIProp.c 				\
	libmapi/IMAPISession.c				\
	libmapi/IMAPISupport.c				\
	libmapi/IMAPITable.c 				\
	libmapi/IMSProvider.c				\
	libmapi/IMessage.c 				\
	libmapi/IMsgStore.c 				\
	libmapi/IStoreFolder.c				\
	libmapi/IUnknown.c 				\
	libmapi/IStream.c				\
	libmapi/IXPLogon.c				\
	libmapi/FXICS.c					\
	libmapi/freebusy.c				\
	libmapi/x500.c 					\
	libmapi/lzfu.c					\
	libmapi/utils.c 				\
	libmapi/util/lcid.c 				\
	libmapi/util/codepage.c 			\
	libmapi/socket/interface.c			\
	libmapi/socket/netif.c		
	@echo "Generating $@"
	@./script/mkproto.pl --private=libmapi/proto_private.h --public=libmapi/proto.h $^

libmapi/emsmdb.c: libmapi/emsmdb.h gen_ndr/ndr_exchange_c.h

libmapi/mapitags.c libmapi/mapicode.c mapitags_enum.h mapicodes_enum.h: \
	libmapi/conf/mapi-properties 					\
	libmapi/conf/mapi-codes 					\
	libmapi/conf/mapi-named-properties 				\
	libmapi/conf/mparse.pl
	@./libmapi/conf/build.sh

#################################################################
# libmapi++ compilation rules
#################################################################

libmapixx: libmapi libmapixx-tests libmapixx-examples

libmapixx-installpc:

libmapixx-clean: libmapixx-tests-clean

libmapixx-install: libmapixx-installheader

libmapixx-uninstall: libmapixx-uninstallheader

libmapixx-installheader:
	@echo "[*] install: libmapi++ headers"
	$(INSTALL) -d $(DESTDIR)$(includedir)/libmapi++
	$(INSTALL) -m 0644 libmapi++/attachment.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/clibmapi.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/folder.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/libmapi++.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/mapi_exception.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/message.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/message_store.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/object.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/profile.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/property_container.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -m 0644 libmapi++/session.h $(DESTDIR)$(includedir)/libmapi++/
	$(INSTALL) -d $(DESTDIR)$(includedir)/libmapi++/impl
	$(INSTALL) -m 0644 libmapi++/impl/* $(DESTDIR)$(includedir)/libmapi++/impl/

libmapixx-uninstallheader:
	rm -rf $(DESTDIR)$(includedir)/libmapi++


libmapixx-tests:	libmapixx-test		\
			libmapixx-attach 	\
			libmapixx-exception

libmapixx-tests-clean:	libmapixx-test-clean	\
			libmapixx-attach-clean	\
			libmapixx-exception-clean 

libmapixx-test: bin/libmapixx-test

libmapixx-test-clean:
	rm -f bin/libmapixx-test
	rm -f libmapi++/tests/*.o

clean:: libmapixx-tests-clean

bin/libmapixx-test:	libmapi++/tests/test.cpp	\
		libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking sample application $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:: libmapixx-test-clean

libmapixx-attach: bin/libmapixx-attach

libmapixx-attach-clean:
	rm -f bin/libmapixx-attach
	rm -f libmapi++/tests/*.o

bin/libmapixx-attach: libmapi++/tests/attach_test.cpp	\
		  libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking sample application $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:: libmapixx-attach-clean

libmapixx-exception: bin/libmapixx-exception
 
bin/libmapixx-exception: libmapi++/tests/exception_test.cpp \
		  libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking exception test application $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

libmapixx-exception-clean:
	rm -f bin/libmapixx-exception
	rm -f libmapi++/tests/*.o

clean:: libmapixx-exception-clean

libmapixx-examples: libmapi++/examples/foldertree \
		  libmapi++/examples/messages

libmapixx-foldertree-clean:
	rm -f libmapi++/examples/foldertree
	rm -f libmapi++/examples/*.o

libmapixx-messages-clean:
	rm -f libmapi++/examples/messages
	rm -f libmapi++/examples/*.o

libmapi++/examples/foldertree: libmapi++/examples/foldertree.cpp	\
		  libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking foldertree example application $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:: libmapixx-foldertree-clean

libmapi++/examples/messages: libmapi++/examples/messages.cpp	\
		  libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking messages example application $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:: libmapixx-messages-clean

#################################################################
# libmapiadmin compilation rules
#################################################################

LIBMAPIADMIN_SO_VERSION = 0

libmapiadmin:	libmapiadmin/proto.h				\
		libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION)

libmapiadmin-install:	libmapiadmin-installpc		\
			libmapiadmin-installlib		\
			libmapiadmin-installheader

libmapiadmin-uninstall:	libmapiadmin-uninstallpc	\
			libmapiadmin-uninstalllib	\
			libmapiadmin-uninstallheader

libmapiadmin-clean::
	rm -f libmapiadmin/*.o libmapiadmin/*.po
	rm -f libmapiadmin/*.gcno libmapiadmin/*.gcda
ifneq ($(SNAPSHOT), no)
	rm -f libmapiadmin/proto.h
	rm -f libmapiadmin/proto_private.h
endif
	rm -f libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION) libmapiadmin.$(SHLIBEXT).$(LIBMAPIADMIN_SO_VERSION) \
		  libmapiadmin.$(SHLIBEXT)

clean:: libmapiadmin-clean

libmapiadmin-distclean::
	rm -f libmapiadmin.pc

distclean:: libmapiadmin-distclean

libmapiadmin-installpc:
	@echo "[*] install: libmapiadmin pc files"
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 0644 libmapiadmin.pc $(DESTDIR)$(libdir)/pkgconfig

libmapiadmin-installlib:
	@echo "[*] install: libmapiadmin library"
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 0755 libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapiadmin.$(SHLIBEXT)
ifeq ($(MANUALLY_CREATE_SYMLINKS), yes)
	ln -sf libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapiadmin.$(SHLIBEXT).$(LIBMAPI_SO_VERSION)
endif

libmapiadmin-installheader:
	@echo "[*] install: libmapiadmin headers"
	$(INSTALL) -d $(DESTDIR)$(includedir)/libmapiadmin 
	$(INSTALL) -m 0644 libmapiadmin/proto.h $(DESTDIR)$(includedir)/libmapiadmin/
	$(INSTALL) -m 0644 libmapiadmin/libmapiadmin.h $(DESTDIR)$(includedir)/libmapiadmin/

libmapiadmin-uninstallpc:
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libmapiadmin.pc

libmapiadmin-uninstalllib:
	rm -f $(DESTDIR)$(libdir)/libmapiadmin.*

libmapiadmin-uninstallheader:
	rm -rf $(DESTDIR)$(includedir)/libmapiadmin

libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION):	\
	libmapiadmin/mapiadmin_user.po		\
	libmapiadmin/mapiadmin.po 		\
	libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) $(DSOOPT) $(LDFLAGS) -Wl,-soname,libmapiadmin.$(SHLIBEXT).$(LIBMAPIADMIN_SO_VERSION) -o $@ $^ $(LIBS) $(LIBMAPIADMIN_LIBS) 

libmapiadmin/proto.h libmapiadmin/proto_private.h: 	\
	libmapiadmin/mapiadmin.c 			\
	libmapiadmin/mapiadmin_user.c			
	@echo "Generating $@"
	@./script/mkproto.pl -private=libmapiadmin/proto_private.h --public=libmapiadmin/proto.h $^



#################################################################
# libocpf compilation rules
#################################################################

LIBOCPF_SO_VERSION = 0

libocpf:	libocpf/proto.h				\
		libocpf.$(SHLIBEXT).$(PACKAGE_VERSION)

libocpf-install:	libocpf-installpc	\
			libocpf-installlib	\
			libocpf-installheader

libocpf-uninstall:	libocpf-uninstallpc	\
			libocpf-uninstalllib	\
			libocpf-uninstallheader

libocpf-clean::
	rm -f libocpf/*.o libocpf/*.po
	rm -f libocpf/*.gcno libocpf/*.gcda
ifneq ($(SNAPSHOT), no)
	rm -f libocpf/lex.yy.c
	rm -f libocpf/ocpf.tab.c libocpf/ocpf.tab.h
	rm -f libocpf/proto.h
	rm -f libocpf/proto_private.h
endif
	rm -f libocpf.$(SHLIBEXT).$(PACKAGE_VERSION) libocpf.$(SHLIBEXT).$(LIBOCPF_SO_VERSION) \
		  libocpf.$(SHLIBEXT)

clean:: libocpf-clean

libocpf-distclean::
	rm -f libocpf.pc

distclean:: libocpf-distclean

libocpf-installpc:
	@echo "[*] install: libocpf pc files"
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 0644 libocpf.pc $(DESTDIR)$(libdir)/pkgconfig

libocpf-installlib:
	@echo "[*] install: libocpf library"
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 0755 libocpf.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libocpf.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libocpf.$(SHLIBEXT)
ifeq ($(MANUALLY_CREATE_SYMLINKS), yes)
	ln -sf libocpf.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libocpf.$(SHLIBEXT).$(LIBMAPI_SO_VERSION)
endif

libocpf-installheader:
	@echo "[*] install: libocpf headers"
	$(INSTALL) -d $(DESTDIR)$(includedir)/libocpf
	$(INSTALL) -m 0644 libocpf/ocpf.h $(DESTDIR)$(includedir)/libocpf/
	$(INSTALL) -m 0644 libocpf/proto.h $(DESTDIR)$(includedir)/libocpf/

libocpf-uninstallpc:
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libocpf.pc

libocpf-uninstalllib:
	rm -f $(DESTDIR)$(libdir)/libocpf.*

libocpf-uninstallheader:
	rm -rf $(DESTDIR)$(includedir)/libocpf

libocpf.$(SHLIBEXT).$(PACKAGE_VERSION):		\
	libocpf/ocpf.tab.po			\
	libocpf/lex.yy.po			\
	libocpf/ocpf_public.po			\
	libocpf/ocpf_dump.po			\
	libocpf/ocpf_api.po			\
	libocpf/ocpf_write.po			\
	libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) $(DSOOPT) $(LDFLAGS) -Wl,-soname,libocpf.$(SHLIBEXT).$(LIBOCPF_SO_VERSION) -o $@ $^ $(LIBS)

libocpf.$(SHLIBEXT).$(LIBOCPF_SO_VERSION): libocpf.$(SHLIBEXT).$(PACKAGE_VERSION)
	ln -fs $< $@

libocpf/proto.h:	libocpf/ocpf_public.c	\
			libocpf/ocpf_dump.c	\
			libocpf/ocpf_api.c	\
			libocpf/ocpf_write.c
			@echo "Generating $@"
			@./script/mkproto.pl --private=libocpf/proto_private.h \
			--public=libocpf/proto.h $^

libocpf/lex.yy.c:		libocpf/lex.l
	@echo "Generating $@"
	@$(FLEX) -t $< > $@

libocpf/ocpf.tab.c:	libocpf/ocpf.y
	@echo "Generating $@"
	@$(BISON) -pocpf_yy -d $< -o $@

# Avoid warnings
libocpf/lex.yy.o: CFLAGS=
libocpf/ocpf.tab.o: CFLAGS=



#################################################################
# torture suite compilation rules
#################################################################

torture:	torture/torture_proto.h		\
		torture/openchange.$(SHLIBEXT)

torture-install:
	@echo "[*] install: openchange torture suite"
	$(INSTALL) -d $(DESTDIR)$(TORTURE_MODULESDIR)
	$(INSTALL) -m 0755 torture/openchange.$(SHLIBEXT) $(DESTDIR)$(TORTURE_MODULESDIR)

torture-uninstall:
	rm -f $(DESTDIR)$(TORTURE_MODULESDIR)/openchange.*

torture-clean::
	rm -f torture/*.$(SHLIBEXT)
ifneq ($(SNAPSHOT), no)
	rm -f torture/torture_proto.h
endif
	rm -f torture/*.o torture/*.po torture/*.gcno torture/*.gcda

clean:: torture-clean

torture/openchange.$(SHLIBEXT):			\
	torture/nspi_profile.po			\
	torture/nspi_resolvenames.po		\
	torture/mapi_restrictions.po		\
	torture/mapi_criteria.po		\
	torture/mapi_copymail.po		\
	torture/mapi_sorttable.po		\
	torture/mapi_bookmark.po		\
	torture/mapi_fetchmail.po		\
	torture/mapi_sendmail.po		\
	torture/mapi_sendmail_html.po		\
	torture/mapi_deletemail.po		\
	torture/mapi_newmail.po			\
	torture/mapi_sendattach.po		\
	torture/mapi_fetchattach.po		\
	torture/mapi_fetchappointment.po	\
	torture/mapi_sendappointment.po		\
	torture/mapi_fetchcontacts.po		\
	torture/mapi_sendcontacts.po		\
	torture/mapi_fetchtasks.po		\
	torture/mapi_sendtasks.po		\
	torture/mapi_common.po			\
	torture/mapi_permissions.po		\
	torture/mapi_createuser.po		\
	torture/exchange_createuser.po		\
	torture/mapi_namedprops.po		\
	torture/mapi_recipient.po		\
	torture/openchange.po			\
	libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS)

torture/torture_proto.h: torture/mapi_restrictions.c	\
	torture/mapi_criteria.c		\
	torture/mapi_deletemail.c	\
	torture/mapi_newmail.c		\
	torture/mapi_fetchmail.c	\
	torture/mapi_sendattach.c 	\
	torture/mapi_sorttable.c	\
	torture/mapi_bookmark.c		\
	torture/mapi_copymail.c		\
	torture/mapi_fetchattach.c	\
	torture/mapi_sendmail.c		\
	torture/mapi_sendmail_html.c	\
	torture/nspi_profile.c 		\
	torture/nspi_resolvenames.c	\
	torture/mapi_fetchappointment.c	\
	torture/mapi_sendappointment.c	\
	torture/mapi_fetchcontacts.c	\
	torture/mapi_sendcontacts.c	\
	torture/mapi_fetchtasks.c	\
	torture/mapi_sendtasks.c	\
	torture/mapi_common.c		\
	torture/mapi_permissions.c	\
	torture/mapi_namedprops.c	\
	torture/mapi_recipient.c	\
	torture/mapi_createuser.c	\
	torture/exchange_createuser.c	\
	torture/openchange.c
	@echo "Generating $@"
	@./script/mkproto.pl --private=torture/torture_proto.h --public=torture/torture_proto.h $^

#################################################################
# mapiproxy compilation rules
#################################################################
LIBMAPIPROXY_SO_VERSION = 0
LIBMAPISERVER_SO_VERSION = 0

.PHONY: mapiproxy

mapiproxy: 		idl 					\
			libmapiproxy				\
			libmapiserver				\
			libmapistore				\
			mapiproxy/dcesrv_mapiproxy.$(SHLIBEXT) 	\
			mapiproxy-modules			\
			mapiproxy-servers

mapiproxy-install: 	mapiproxy				\
			mapiproxy-modules-install		\
			mapiproxy-servers-install		\
			libmapiproxy-install			\
			libmapiserver-install			\
			libmapistore-install
	$(INSTALL) -d $(DESTDIR)$(SERVER_MODULESDIR)
	$(INSTALL) -m 0755 mapiproxy/dcesrv_mapiproxy.$(SHLIBEXT) $(DESTDIR)$(SERVER_MODULESDIR)

mapiproxy-uninstall: 	mapiproxy-modules-uninstall		\
			mapiproxy-servers-uninstall		\
			libmapiproxy-uninstall			\
			libmapiserver-uninstall			\
			libmapistore-uninstall
	rm -f $(DESTDIR)$(SERVER_MODULESDIR)/dcesrv_mapiproxy.*
	rm -f $(DESTDIR)$(libdir)/libmapiproxy.*
	rm -f $(DESTDIR)$(includedir)/libmapiproxy.h

mapiproxy-clean:: 	mapiproxy-modules-clean			\
			mapiproxy-servers-clean			\
			libmapiproxy-clean			\
			libmapiserver-clean			\
			libmapistore-clean
	rm -f mapiproxy/*.o mapiproxy/*.po
	rm -f mapiproxy/*.gcno mapiproxy/*.gcda
	rm -f mapiproxy/dcesrv_mapiproxy.$(SHLIBEXT)

clean:: mapiproxy-clean


mapiproxy/dcesrv_mapiproxy.$(SHLIBEXT): 	mapiproxy/dcesrv_mapiproxy.po		\
						mapiproxy/dcesrv_mapiproxy_nspi.po	\
						mapiproxy/dcesrv_mapiproxy_rfr.po	\
						mapiproxy/dcesrv_mapiproxy_unused.po	\
						ndr_mapi.po				\
						gen_ndr/ndr_exchange.po				

	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/dcesrv_mapiproxy.c: gen_ndr/ndr_exchange_s.c gen_ndr/ndr_exchange.c


###############
# libmapiproxy
###############

libmapiproxy: mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

libmapiproxy-install:
	$(INSTALL) -m 0755 mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapiproxy.$(SHLIBEXT)
	$(INSTALL) -m 0644 mapiproxy/libmapiproxy/libmapiproxy.h $(DESTDIR)$(includedir)/
	$(INSTALL) -m 0644 mapiproxy/libmapiproxy.pc $(DESTDIR)$(libdir)/pkgconfig

libmapiproxy-clean:
	rm -f mapiproxy/libmapiproxy/*.po mapiproxy/libmapiproxy/*.o
	rm -f mapiproxy/libmapiproxy/*.gcno mapiproxy/libmapiproxy/*.gcda
ifneq ($(SNAPSHOT), no)
	rm -f mapiproxy/libmapiproxy/openchangedb_property.c
endif
	rm -f mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)
	rm -f mapiproxy/libmapiproxy.$(SHLIBEXT).$(LIBMAPIPROXY_SO_VERSION)

libmapiproxy-uninstall:
	rm -f $(DESTDIR)$(libdir)/libmapiproxy.*
	rm -f $(DESTDIR)$(includedir)/libmapiproxy.h
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libmapiproxy.pc

libmapiproxy-distclean:
	rm -f mapiproxy/libmapiproxy.pc

distclean::libmapiproxy-distclean

mapiproxy/libmapiproxy/openchangedb_property.c: libmapi/conf/mapi-properties libmapi/conf/mparse.pl
	@./libmapi/conf/mparse.pl --parser=openchangedb_property --outputdir=mapiproxy/libmapiproxy/ \
				  libmapi/conf/mapi-properties

mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION):	mapiproxy/libmapiproxy/dcesrv_mapiproxy_module.po	\
							mapiproxy/libmapiproxy/dcesrv_mapiproxy_server.po	\
							mapiproxy/libmapiproxy/dcesrv_mapiproxy_session.po	\
							mapiproxy/libmapiproxy/openchangedb.po			\
							mapiproxy/libmapiproxy/openchangedb_property.po		\
							mapiproxy/libmapiproxy/mapi_handles.po			\
							mapiproxy/libmapiproxy/entryid.po			\
							libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@$(CC) -o $@ $(DSOOPT) -Wl,-soname,libmapiproxy.$(SHLIBEXT).$(LIBMAPIPROXY_SO_VERSION) $^ -L. $(LIBS) $(TDB_LIBS)

mapiproxy/libmapiproxy.$(SHLIBEXT).$(LIBMAPIPROXY_SO_VERSION): libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)
	ln -fs $< $@


#################
# libmapiserver
#################

libmapiserver: mapiproxy/libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION)

libmapiserver-install:
	$(INSTALL) -m 0755 mapiproxy/libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapiserver.$(SHLIBEXT)
	$(INSTALL) -m 0644 mapiproxy/libmapiserver/libmapiserver.h $(DESTDIR)$(includedir)/
	$(INSTALL) -m 0644 mapiproxy/libmapiserver.pc $(DESTDIR)$(libdir)/pkgconfig

libmapiserver-clean:
	rm -f mapiproxy/libmapiserver/*.po mapiproxy/libmapiserver/*.o
	rm -f mapiproxy/libmapiserver/*.gcno mapiproxy/libmapiserver/*.gcda
	rm -f mapiproxy/libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION)
	rm -f mapiproxy/libmapiserver.$(SHLIBEXT).$(LIBMAPISERVER_SO_VERSION)

libmapiserver-uninstall:
	rm -f $(DESTDIR)$(libdir)/libmapiserver.*
	rm -f $(DESTDIR)$(includedir)/libmapiserver.h
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libmapiserver.pc

libmapiserver-distclean:
	rm -f mapiproxy/libmapiserver.pc

distclean:: libmapiserver-distclean

mapiproxy/libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION):	mapiproxy/libmapiserver/libmapiserver_oxcstor.po	\
							mapiproxy/libmapiserver/libmapiserver_oxcprpt.po	\
							mapiproxy/libmapiserver/libmapiserver_oxcfold.po	\
							mapiproxy/libmapiserver/libmapiserver_oxctabl.po	\
							mapiproxy/libmapiserver/libmapiserver_oxcmsg.po		\
							mapiproxy/libmapiserver/libmapiserver_oxcnotif.po	\
							mapiproxy/libmapiserver/libmapiserver_oxorule.po	 \
							ndr_mapi.po				\
							gen_ndr/ndr_exchange.po
	@$(CC) -o $@ $(DSOOPT) -Wl,-soname,libmapiserver.$(SHLIBEXT).$(LIBMAPIPROXY_SO_VERSION) $^ $(LIBS)

mapiproxy/libmapiserver.$(SHLIBEXT).$(LIBMAPISERVER_SO_VERSION): libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION)
	ln -fs $< $@


################
# libmapistore
################
LIBMAPISTORE_SO_VERSION = 0

libmapistore: 	mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)	\
		$(OC_MAPISTORE)						\
		mapistore_test

libmapistore-install:	$(OC_MAPISTORE_INSTALL)
	$(INSTALL) -m 0755 mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)
	ln -sf libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION) $(DESTDIR)$(libdir)/libmapistore.$(SHLIBEXT)
	$(INSTALL) -d $(DESTDIR)$(includedir)/mapistore
	$(INSTALL) -m 0644 mapiproxy/libmapistore/mapistore.h $(DESTDIR)$(includedir)/mapistore/
	$(INSTALL) -m 0644 mapiproxy/libmapistore/mapistore_errors.h $(DESTDIR)$(includedir)/mapistore/
	$(INSTALL) -m 0644 mapiproxy/libmapiserver.pc $(DESTDIR)$(libdir)/pkgconfig

libmapistore-clean:	$(OC_MAPISTORE_CLEAN)
	rm -f mapiproxy/libmapistore/*.po mapiproxy/libmapistore/*.o
	rm -f mapiproxy/libmapistore/*.gcno mapiproxy/libmapistore/*.gcda
	rm -f mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)
	rm -f mapiproxy/libmapistore.$(SHLIBEXT).$(LIBMAPISTORE_SO_VERSION)

libmapistore-uninstall:	$(OC_MAPISTORE_UNINSTALL)
	rm -f $(DESTDIR)$(libdir)/libmapistore.*
	rm -rf $(DESTDIR)$(includedir)/mapistore
	rm -f $(DESTDIR)$(libdir)/pkgconfig/libmapistore.pc

libmapistore-distclean: libmapistore-clean
	rm -f mapiproxy/libmapistore.pc

distclean:: libmapistore-distclean

mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION): 	mapiproxy/libmapistore/mapistore_interface.po	\
							mapiproxy/libmapistore/mapistore_processing.po	\
							mapiproxy/libmapistore/mapistore_backend.po	\
							mapiproxy/libmapistore/mapistore_tdb_wrap.po
	@$(CC) -o $@ $(DSOOPT) $(LDFLAGS) -Wl,-soname,libmapistore.$(SHLIBEXT).$(LIBMAPISTORE_SO_VERSION) $^ -L. $(LIBS) $(TDB_LIBS)

mapiproxy/libmapistore.$(SHLIBEXT).$(LIBMAPISTORE_SO_VERSION): libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)
	ln -fs $< $@

#####################
# mapistore backends
#####################

mapistore_sqlite3: mapiproxy/libmapistore/backends/mapistore_sqlite3.$(SHLIBEXT)

mapistore_sqlite3-install:
	$(INSTALL) -d $(DESTDIR)$(libdir)/mapistore_backends
	$(INSTALL) -m 0755 mapiproxy/libmapistore/backends/mapistore_sqlite3.$(SHLIBEXT) $(DESTDIR)$(libdir)/mapistore_backends/

mapistore_sqlite3-uninstall:
	rm -rf $(DESTDIR)$(libdir)/mapistore_backends

mapistore_sqlite3-clean:
	rm -f mapiproxy/libmapistore/backends/mapistore_sqlite3.o
	rm -f mapiproxy/libmapistore/backends/mapistore_sqlite3.po
	rm -f mapiproxy/libmapistore/backends/mapistore_sqlite3.gcno
	rm -f mapiproxy/libmapistore/backends/mapistore_sqlite3.gcda

clean:: mapistore_sqlite3-clean

mapistore_sqlite3-distclean: mapistore_sqlite3-clean
	rm -f mapiproxy/libmapistore/backends/mapistore_sqlite3.so

distclean:: mapistore_sqlite3-distclean

mapiproxy/libmapistore/backends/mapistore_sqlite3.$(SHLIBEXT): mapiproxy/libmapistore/backends/mapistore_sqlite3.po
	@echo "Linking mapistore module $@"
	@$(CC) $(SQLITE_CFLAGS) -o $@ $(DSOOPT) $(LDFLAGS) $^ -L. $(LIBS) $(SQLITE_LIBS) 	\
	-Lmapiproxy mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)

#######################
# mapistore test tools
#######################

mapistore_test: bin/mapistore_test

bin/mapistore_test: 	mapiproxy/libmapistore/tests/mapistore_test.o		\
			mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt

mapistore_clean:
	rm -f mapiproxy/libmapistore/tests/*.o
	rm -f mapiproxy/libmapistore/tests/*.gcno
	rm -f mapiproxy/libmapistore/tests/*.gcda
	rm -f bin/mapistore_test

clean:: mapistore_clean

####################
# mapiproxy modules
####################

mapiproxy-modules:	mapiproxy/modules/mpm_downgrade.$(SHLIBEXT)	\
			mapiproxy/modules/mpm_pack.$(SHLIBEXT)		\
			mapiproxy/modules/mpm_cache.$(SHLIBEXT)		\
			mapiproxy/modules/mpm_dummy.$(SHLIBEXT)		

mapiproxy-modules-install: mapiproxy-modules
	$(INSTALL) -d $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy/
	$(INSTALL) -m 0755 mapiproxy/modules/mpm_downgrade.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy/
	$(INSTALL) -m 0755 mapiproxy/modules/mpm_pack.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy/
	$(INSTALL) -m 0755 mapiproxy/modules/mpm_cache.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy/
	$(INSTALL) -m 0755 mapiproxy/modules/mpm_dummy.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy/

mapiproxy-modules-uninstall:
	rm -rf $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy

mapiproxy-modules-clean::
	rm -f mapiproxy/modules/*.o mapiproxy/modules/*.po
	rm -f mapiproxy/modules/*.gcno mapiproxy/modules/*.gcda
	rm -f mapiproxy/modules/*.so

clean:: mapiproxy-modules-clean

mapiproxy/modules/mpm_downgrade.$(SHLIBEXT): mapiproxy/modules/mpm_downgrade.po
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/modules/mpm_pack.$(SHLIBEXT):	mapiproxy/modules/mpm_pack.po	\
					ndr_mapi.po			\
					gen_ndr/ndr_exchange.po
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/modules/mpm_cache.$(SHLIBEXT): mapiproxy/modules/mpm_cache.po		\
					 mapiproxy/modules/mpm_cache_ldb.po	\
					 mapiproxy/modules/mpm_cache_stream.po	\
					 ndr_mapi.po				\
					 gen_ndr/ndr_exchange.po
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/modules/mpm_dummy.$(SHLIBEXT): mapiproxy/modules/mpm_dummy.po
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)


####################
# mapiproxy servers
####################
provision-install: python-install
	$(INSTALL) -d $(DESTDIR)$(datadir)/setup/AD
	$(INSTALL) -m 0644 setup/AD/oc_provision* $(DESTDIR)$(datadir)/setup/AD/
	$(INSTALL) -m 0644 setup/AD/prefixMap.txt $(DESTDIR)$(datadir)/setup/AD/
	$(INSTALL) -d $(DESTDIR)$(datadir)/setup/openchangedb
	$(INSTALL) -m 0644 setup/openchangedb/oc_provision* $(DESTDIR)$(datadir)/setup/openchangedb/

provision-uninstall: python-uninstall
	rm -f $(DESTDIR)$(datadir)/setup/AD/oc_provision_configuration.ldif
	rm -f $(DESTDIR)$(datadir)/setup/AD/oc_provision_schema.ldif
	rm -f $(DESTDIR)$(datadir)/setup/AD/oc_provision_schema_modify.ldif
	rm -f $(DESTDIR)$(datadir)/setup/AD/oc_provision_schema_ADSC.ldif
	rm -f $(DESTDIR)$(datadir)/setup/AD/prefixMap.txt
	rm -rf $(DESTDIR)$(datadir)/setup/AD
	rm -rf $(DESTDIR)$(datadir)/setup/openchangedb

mapiproxy-servers:	mapiproxy/servers/exchange_nsp.$(SHLIBEXT)		\
			mapiproxy/servers/exchange_emsmdb.$(SHLIBEXT)		\
			mapiproxy/servers/exchange_ds_rfr.$(SHLIBEXT)

mapiproxy-servers-install: mapiproxy-servers provision-install
	$(INSTALL) -d $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy_server/
	$(INSTALL) -m 0755 mapiproxy/servers/exchange_nsp.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy_server/
	$(INSTALL) -m 0755 mapiproxy/servers/exchange_emsmdb.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy_server/
	$(INSTALL) -m 0755 mapiproxy/servers/exchange_ds_rfr.$(SHLIBEXT) $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy_server/

mapiproxy-servers-uninstall: provision-uninstall
	rm -rf $(DESTDIR)$(modulesdir)/dcerpc_mapiproxy_server

mapiproxy-servers-clean::
	rm -f mapiproxy/servers/default/nspi/*.o mapiproxy/servers/default/nspi/*.po
	rm -f mapiproxy/servers/default/nspi/*.gcno mapiproxy/servers/default/nspi/*.gcda
	rm -f mapiproxy/servers/default/emsmdb/*.o mapiproxy/servers/default/emsmdb/*.po
	rm -f mapiproxy/servers/default/emsmdb/*.gcno mapiproxy/servers/default/emsmdb/*.gcda
	rm -f mapiproxy/servers/default/rfr/*.o mapiproxy/servers/default/rfr/*.po
	rm -f mapiproxy/servers/default/rfr/*.gcno mapiproxy/servers/default/rfr/*.gcda
	rm -f mapiproxy/servers/*.so

clean:: mapiproxy-servers-clean

mapiproxy/servers/exchange_nsp.$(SHLIBEXT):	mapiproxy/servers/default/nspi/dcesrv_exchange_nsp.po	\
						mapiproxy/servers/default/nspi/emsabp.po		\
						mapiproxy/servers/default/nspi/emsabp_tdb.po		\
						mapiproxy/servers/default/nspi/emsabp_property.po	
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/servers/exchange_emsmdb.$(SHLIBEXT):	mapiproxy/servers/default/emsmdb/dcesrv_exchange_emsmdb.po	\
						mapiproxy/servers/default/emsmdb/emsmdbp.po			\
						mapiproxy/servers/default/emsmdb/emsmdbp_object.po		\
						mapiproxy/servers/default/emsmdb/oxcstor.po			\
						mapiproxy/servers/default/emsmdb/oxcprpt.po			\
						mapiproxy/servers/default/emsmdb/oxcfold.po			\
						mapiproxy/servers/default/emsmdb/oxctabl.po			\
						mapiproxy/servers/default/emsmdb/oxcmsg.po			\
						mapiproxy/servers/default/emsmdb/oxcnotif.po			\
						mapiproxy/servers/default/emsmdb/oxorule.po			
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION) \
						mapiproxy/libmapiserver.$(SHLIBEXT).$(PACKAGE_VERSION)		\
						mapiproxy/libmapistore.$(SHLIBEXT).$(PACKAGE_VERSION)

mapiproxy/servers/exchange_ds_rfr.$(SHLIBEXT):	mapiproxy/servers/default/rfr/dcesrv_exchange_ds_rfr.po
	@echo "Linking $@"
	@$(CC) -o $@ $(DSOOPT) $^ -L. $(LIBS) -Lmapiproxy mapiproxy/libmapiproxy.$(SHLIBEXT).$(PACKAGE_VERSION)

#################################################################
# Tools compilation rules
#################################################################

###################
# openchangeclient
###################

openchangeclient:	bin/openchangeclient

openchangeclient-install:	openchangeclient
	$(INSTALL) -d $(DESTDIR)$(bindir) 
	$(INSTALL) -m 0755 bin/openchangeclient $(DESTDIR)$(bindir)

openchangeclient-uninstall:
	rm -f $(DESTDIR)$(bindir)/openchangeclient

openchangeclient-clean::
	rm -f bin/openchangeclient
	rm -f utils/openchangeclient.o
	rm -f utils/openchangeclient.gcno
	rm -f utils/openchangeclient.gcda
	rm -f utils/openchange-tools.o
	rm -f utils/openchange-tools.gcno
	rm -f utils/openchange-tools.gcda

clean:: openchangeclient-clean

bin/openchangeclient: 	utils/openchangeclient.o			\
			utils/openchange-tools.o			\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)		\
			libocpf.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt


##############
# mapiprofile
##############

mapiprofile:		bin/mapiprofile

mapiprofile-install:	mapiprofile
	$(INSTALL) -d $(DESTDIR)$(bindir) 
	$(INSTALL) -m 0755 bin/mapiprofile $(DESTDIR)$(bindir)

mapiprofile-uninstall:
	rm -f $(DESTDIR)$(bindir)/mapiprofile

mapiprofile-clean::
	rm -f bin/mapiprofile
	rm -f utils/mapiprofile.o
	rm -f utils/mapiprofile.gcno
	rm -f utils/mapiprofile.gcda

clean:: mapiprofile-clean

bin/mapiprofile: 	utils/mapiprofile.o 			\
			utils/openchange-tools.o 		\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt


###################
#openchangepfadmin
###################

openchangepfadmin:	bin/openchangepfadmin

openchangepfadmin-install:	openchangepfadmin
	$(INSTALL) -d $(DESTDIR)$(bindir) 
	$(INSTALL) -m 0755 bin/openchangepfadmin $(DESTDIR)$(bindir)

openchangepfadmin-uninstall:
	rm -f $(DESTDIR)$(bindir)/openchangepfadmin

openchangepfadmin-clean::
	rm -f bin/openchangepfadmin
	rm -f utils/openchangepfadmin.o
	rm -f utils/openchangepfadmin.gcno
	rm -f utils/openchangepfadmin.gcda

clean:: openchangepfadmin-clean

bin/openchangepfadmin:	utils/openchangepfadmin.o			\
			utils/openchange-tools.o			\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION) 		\
			libmapiadmin.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBMAPIADMIN_LIBS) -lpopt			


###################
# exchange2mbox
###################

exchange2mbox:		bin/exchange2mbox

exchange2mbox-install:	exchange2mbox
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/exchange2mbox $(DESTDIR)$(bindir)

exchange2mbox-uninstall:
	rm -f $(DESTDIR)$(bindir)/exchange2mbox

exchange2mbox-clean::
	rm -f bin/exchange2mbox
	rm -f utils/exchange2mbox.o
	rm -f utils/exchange2mbox.gcno
	rm -f utils/exchange2mbox.gcda
	rm -f utils/openchange-tools.o
	rm -f utils/openchange-tools.gcno
	rm -f utils/openchange-tools.gcda

clean:: exchange2mbox-clean

bin/exchange2mbox:	utils/exchange2mbox.o				\
			utils/openchange-tools.o			\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) -lpopt  $(MAGIC_LIBS)


###################
# exchange2ical
###################

exchange2ical:		bin/exchange2ical

exchange2ical-install:	exchange2ical
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/exchange2ical $(DESTDIR)$(bindir)

exchange2ical-uninstall:
	rm -f $(DESTDIR)$(bindir)/exchange2ical

exchange2ical-clean::
	rm -f bin/exchange2ical
	rm -f utils/exchange2ical_tool.o
	rm -f utils/exchange2ical_tool.gcno
	rm -f utils/exchange2ical_tool.gcda
	rm -f libexchange2ical/libexchange2ical.o
	rm -f libexchange2ical/libexchange2ical.gcno
	rm -f libexchange2ical/libexchange2ical.gcda
	rm -f libexchange2ical/exchange2ical.o
	rm -f libexchange2ical/exchange2ical.gcno
	rm -f libexchange2ical/exchange2ical.gcda
	rm -f libexchange2ical/exchange2ical_utils.o
	rm -f libexchange2ical/exchange2ical_utils.gcno
	rm -f libexchange2ical/exchange2ical_utils.gcda
	rm -f libexchange2ical/exchange2ical_component.o
	rm -f libexchange2ical/exchange2ical_component.gcno
	rm -f libexchange2ical/exchange2ical_component.gcda
	rm -f libexchange2ical/exchange2ical_property.o
	rm -f libexchange2ical/exchange2ical_property.gcno
	rm -f libexchange2ical/exchange2ical_property.gcda
	rm -f libexchange2ical/libical2exchange.o
	rm -f libexchange2ical/libical2exchange.gcno
	rm -f libexchange2ical/libical2exchange.gcda
	rm -f libexchange2ical/ical2exchange.o
	rm -f libexchange2ical/ical2exchange.gcno
	rm -f libexchange2ical/ical2exchange.gcda
	rm -f libexchange2ical/ical2exchange_property.o
	rm -f libexchange2ical/ical2exchange_property.gcno
	rm -f libexchange2ical/ical2exchange_property.gcda
	rm -f libexchange2ical/openchange-tools.o
	rm -f libexchange2ical/openchange-tools.gcno
	rm -f libexchange2ical/openchange-tools.gcda


clean:: exchange2ical-clean

bin/exchange2ical:	utils/exchange2ical_tool.o	\
			libexchange2ical/libexchange2ical.o		\
			libexchange2ical/exchange2ical.o		\
			libexchange2ical/exchange2ical_component.o	\
			libexchange2ical/exchange2ical_property.o	\
			libexchange2ical/exchange2ical_utils.o		\
			libexchange2ical/libical2exchange.o	\
			libexchange2ical/ical2exchange.o	\
			libexchange2ical/ical2exchange_property.o	\
			utils/openchange-tools.o			\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) $(ICAL_LIBS) -lpopt


###################
# mapitest
###################

mapitest:	libmapi			\
		utils/mapitest/proto.h 	\
		bin/mapitest

mapitest-install:	mapitest
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/mapitest $(DESTDIR)$(bindir)

mapitest-uninstall:
	rm -f $(DESTDIR)$(bindir)/mapitest

mapitest-clean:
	rm -f bin/mapitest
	rm -f utils/mapitest/*.o
	rm -f utils/mapitest/*.gcno
	rm -f utils/mapitest/*.gcda
	rm -f utils/mapitest/modules/*.o
	rm -f utils/mapitest/modules/*.gcno
	rm -f utils/mapitest/modules/*.gcda
ifneq ($(SNAPSHOT), no)
	rm -f utils/mapitest/proto.h
	rm -f utils/mapitest/mapitest_proto.h
endif

clean:: mapitest-clean

bin/mapitest:	utils/mapitest/mapitest.o			\
		utils/openchange-tools.o			\
		utils/mapitest/mapitest_suite.o			\
		utils/mapitest/mapitest_print.o			\
		utils/mapitest/mapitest_stat.o			\
		utils/mapitest/mapitest_common.o		\
		utils/mapitest/module.o				\
		utils/mapitest/modules/module_oxcstor.o		\
		utils/mapitest/modules/module_oxcfold.o		\
		utils/mapitest/modules/module_oxomsg.o		\
		utils/mapitest/modules/module_oxcmsg.o		\
		utils/mapitest/modules/module_oxcprpt.o		\
		utils/mapitest/modules/module_oxctable.o	\
		utils/mapitest/modules/module_oxorule.o		\
		utils/mapitest/modules/module_oxcnotif.o	\
		utils/mapitest/modules/module_oxcfxics.o	\
		utils/mapitest/modules/module_nspi.o		\
		utils/mapitest/modules/module_noserver.o	\
		utils/mapitest/modules/module_errorchecks.o	\
		utils/mapitest/modules/module_lcid.o		\
		utils/mapitest/modules/module_mapidump.o	\
		libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)		
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt

utils/mapitest/proto.h:					\
	utils/mapitest/mapitest_suite.c			\
	utils/mapitest/mapitest_print.c			\
	utils/mapitest/mapitest_stat.c			\
	utils/mapitest/mapitest_common.c		\
	utils/mapitest/module.c				\
	utils/mapitest/modules/module_oxcstor.c		\
	utils/mapitest/modules/module_oxcfold.c		\
	utils/mapitest/modules/module_oxomsg.c		\
	utils/mapitest/modules/module_oxcmsg.c		\
	utils/mapitest/modules/module_oxcprpt.c		\
	utils/mapitest/modules/module_oxctable.c	\
	utils/mapitest/modules/module_oxorule.c		\
	utils/mapitest/modules/module_oxcnotif.c	\
	utils/mapitest/modules/module_oxcfxics.c	\
	utils/mapitest/modules/module_nspi.c		\
	utils/mapitest/modules/module_noserver.c	\
	utils/mapitest/modules/module_errorchecks.c	\
	utils/mapitest/modules/module_lcid.c		\
	utils/mapitest/modules/module_mapidump.c
	@echo "Generating $@"
	@./script/mkproto.pl --private=utils/mapitest/mapitest_proto.h --public=utils/mapitest/proto.h $^

#####################
# openchangemapidump
#####################

openchangemapidump:		bin/openchangemapidump

openchangemapidump-install:	openchangemapidump
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/openchangemapidump $(DESTDIR)$(bindir)

openchangemapidump-uninstall:
	rm -f bin/openchangemapidump
	rm -f $(DESTDIR)$(bindir)/openchangemapidump

openchangemapidump-clean::
	rm -f bin/openchangemapidump
	rm -f utils/backup/openchangemapidump.o
	rm -f utils/backup/openchangemapidump.gcno
	rm -f utils/backup/openchangemapidump.gcda
	rm -f utils/backup/openchangebackup.o
	rm -f utils/backup/openchangebackup.gcno
	rm -f utils/backup/openchangebackup.gcda

clean:: openchangemapidump-clean

bin/openchangemapidump:	utils/backup/openchangemapidump.o		\
			utils/backup/openchangebackup.o			\
			utils/openchange-tools.o			\
			libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt


###############
# schemaIDGUID
###############

schemaIDGUID:		bin/schemaIDGUID

schemaIDGUID-install:	schemaIDGUID
	$(INSTALL) -m 0755 bin/schemaIDGUID $(DESTDIR)$(bindir)

schemaIDGUID-uninstall:
	rm -f $(DESTDIR)$(bindir)/schemaIDGUID

schemaIDGUID-clean::
	rm -f bin/schemaIDGUID
	rm -f utils/schemaIDGUID.o
	rm -f utils/schemaIDGUID.gcno
	rm -f utils/schemaIDGUID.gcda

clean:: schemaIDGUID-clean

bin/schemaIDGUID: utils/schemaIDGUID.o
	@echo "Linking $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LIBS)


##################
# locale_codepage
##################

locale_codepage:	bin/locale_codepage

locale_codepage-install:	locale_codepage
	$(INSTALL) -m 0755 bin/locale_codepage $(DESTDIR)$(bindir)

locale_codepage-uninstall:
	rm -f bin/locale_codepage
	rm -f $(DESTDIR)$(bindir)/locale_codepage

locale_codepage-clean::
	rm -f bin/locale_codepage
	rm -f libmapi/tests/locale_codepage.o

clean:: locale_codepage-clean

bin/locale_codepage: libmapi/tests/locale_codepage.o libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) -lpopt

###################
# python code
###################

pythonscriptdir = python

pymapi: $(pythonscriptdir)/mapi.$(SHLIBEXT)

pymapi/%: CFLAGS+=`$(PYTHON_CONFIG) --cflags` -fPIC

$(pythonscriptdir)/mapi.$(SHLIBEXT): $(patsubst %.c,%.o,$(wildcard pymapi/*.c)) libmapi.$(SHLIBEXT).$(PACKAGE_VERSION)
	$(CC) -o $@ $^ `$(PYTHON_CONFIG) --libs` $(DSOOPT)

pymapi-install::
	$(INSTALL) -d $(DESTDIR)$(PYCDIR)
	$(INSTALL) -m 0755 $(pythonscriptdir)/mapi.$(SHLIBEXT) $(DESTDIR)$(PYCDIR)

pymapi-uninstall::
	rm -f $(DESTDIR)$(PYCDIR)/mapi.$(SHLIBEXT)

PYTHON_MODULES = $(patsubst $(pythonscriptdir)/%,%,$(shell find  $(pythonscriptdir) -name "*.py"))

python-install::
	@echo "Installing Python modules"
	@$(foreach MODULE, $(PYTHON_MODULES), \
		$(INSTALL) -d $(DESTDIR)$(pythondir)/$(dir $(MODULE)); \
		$(INSTALL) -m 0644 $(pythonscriptdir)/$(MODULE) $(DESTDIR)$(pythondir)/$(dir $(MODULE)); \
	)

python-uninstall::
	rm -rf $(DESTDIR)$(pythondir)/openchange

EPYDOC_OPTIONS = --no-private --url http://www.openchange.org/ --no-sourcecode

epydoc::
	PYTHONPATH=$(pythonscriptdir):$(PYTHONPATH) epydoc $(EPYDOC_OPTIONS) openchange

check-python:
	PYTHONPATH=$(pythonscriptdir):$(PYTHONPATH) trial openchange

check:: check-python

###################
# nagios plugin
###################

nagiosdir = $(libdir)/nagios

installnagios:
	$(INSTALL) -d $(DESTDIR)$(nagiosdir)
	$(INSTALL) -m 0755 script/check_exchange $(DESTDIR)$(nagiosdir)

###################
# libmapi examples
###################
examples:
	cd doc/examples && make && cd ${OLD_PWD}

examples-clean::
	rm -f doc/examples/mapi_sample1
	rm -f doc/examples/fetchappointment
	rm -f doc/examples/fetchmail

clean:: examples-clean

examples-install examples-uninstall:

manpages = \
		doc/man/man1/exchange2mbox.1				\
		doc/man/man1/mapiprofile.1				\
		doc/man/man1/openchangeclient.1				\
		doc/man/man1/openchangepfadmin.1			\
		$(wildcard apidocs/man/man3/*)

installman: doxygen
	@./script/installman.sh $(DESTDIR)$(mandir) $(manpages)


uninstallman:
	@./script/uninstallman.sh $(DESTDIR)$(mandir) $(manpages)

doxygen:	
	@if test ! -d apidocs ; then						\
		echo "Doxify API documentation: HTML and man pages";		\
		mkdir -p apidocs/html;						\
		mkdir -p apidocs/man;						\
		$(DOXYGEN) Doxyfile;						\
		$(DOXYGEN) libmapi/Doxyfile;					\
		$(DOXYGEN) libmapiadmin/Doxyfile;				\
		$(DOXYGEN) libocpf/Doxyfile;					\
		$(DOXYGEN) libmapi++/Doxyfile;					\
		$(DOXYGEN) mapiproxy/Doxyfile;					\
		$(DOXYGEN) utils/mapitest/Doxyfile;				\
		cp -f doc/doxygen/index.html apidocs/html;			\
		cp -f doc/doxygen/pictures/* apidocs/html/overview;		\
		cp -f doc/doxygen/pictures/* apidocs/html/libmapi;		\
		cp -f doc/doxygen/pictures/* apidocs/html/libmapiadmin;		\
		cp -f doc/doxygen/pictures/* apidocs/html/libmapi++;		\
		cp -f doc/doxygen/pictures/* apidocs/html/libocpf;		\
		cp -f doc/doxygen/pictures/* apidocs/html/mapitest;		\
		cp -f doc/doxygen/pictures/* apidocs/html/mapiproxy;		\
		cp -f mapiproxy/documentation/pictures/* apidocs/html/mapiproxy;\
		rm -f apidocs/man/man3/todo.3;					\
		rm -f apidocs/man/man3/bug.3;					\
		rm -f apidocs/man/man3/*.c.3;					\
	fi								

etags:
	etags `find $(srcdir) -name "*.[ch]"`

ctags:
	ctags `find $(srcdir) -name "*.[ch]"`

swigperl-all:
	@echo "Creating Perl bindings ..."
	@$(MAKE) -C swig/perl all

swigperl-install:
	@echo "Install Perl bindings ..."
	@$(MAKE) -C swig/perl install

swigperl-uninstall:
	@echo "Uninstall Perl bindings ..."
	@$(MAKE) -C swig/perl uninstall

distclean::
	@$(MAKE) -C swig/perl distclean

clean::
	@echo "Cleaning Perl bindings ..."
	@$(MAKE) -C swig/perl clean

.PRECIOUS: exchange.h gen_ndr/ndr_exchange.h gen_ndr/ndr_exchange.c gen_ndr/ndr_exchange_c.c gen_ndr/ndr_exchange_c.h

test:: check

check:: torture/openchange.$(SHLIBEXT) libmapi.$(SHLIBEXT).$(LIBMAPI_SO_VERSION)
	# FIXME: Set up server
	LD_LIBRARY_PATH=`pwd` $(SMBTORTURE) --load-module torture/openchange.$(SHLIBEXT) ncalrpc: OPENCHANGE
	./bin/mapitest --mapi-calls 

####################################
# coverage tests
#
# this could be better integrated...
####################################
coverage-init:
	lcov --base-directory . --directory . --capture --initial --output-file oc_cov.info

coverage::
	rm -f libmapi/\<stdout\>.gcov
	rm -f ./libocpf/lex.yy.gcda
	rm -f libocpf/\<stdout\>.gcov
	rm -f ./\<stdout\>.gcov
	lcov --base-directory .  --directory . --output-file oc_cov.info --capture
	genhtml -o covresults oc_cov.info

coverage-clean::
	rm -rf oc_cov.info
	rm -rf covresults
	rm -f test.gcno

clean:: coverage-clean

# This should be the last line in the makefile since other distclean rules may 
# need config.mk
distclean::
	rm -f config.mk
