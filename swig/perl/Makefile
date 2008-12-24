#Makefile for libmapi Perl bindings
#Writen by Julien Kerihuel <j.kerihuel@openchange.org>, 2007.

include ../../config.mk

top_builddir = ../..

CFLAGS+=`perl -MExtUtils::Embed -e ccopts` -D_SAMBA_UTIL_H_

# Portability hack...
CFLAGS+=-Duint_t="unsigned int" 

CFLAGS+=-I$(top_builddir)

all::	swig_mapitags.h 		\
	swig_mapicodes.h		\
	mapi_wrap.c			\
	mapi.$(SHLIBEXT)

clean::
	rm -f *.o
	rm -f *.pm
	rm -f mapi_wrap.c
	rm -f swig_mapicodes.h swig_mapitags.h
	rm -f mapi.$(SHLIBEXT)

distclean:: clean
	rm -f *.so

install:: all
	cp mapi.pm $(PERL5DIR)
	cp mapi.so $(PERL5DIR)

uninstall::
	rm -f $(PERL5DIR)/mapi.pm
	rm -f $(PERL5DIR)/mapi.so

mapi_wrap.c: mapi.i
	@echo "Swigify $<"
	@$(SWIG) -Wall -Werror -perl5 $<

swig_mapitags.h:
	@echo "Generating Headers (swig_mapitags.h)"
	@$(top_builddir)/libmapi/conf/mparse.pl --parser=swig_mapitags --outputdir=./ $(top_builddir)/libmapi/conf/mapi-properties

swig_mapicodes.h:
	@echo "Generating Headers (swig_mapicodes.h)"
	@$(top_builddir)/libmapi/conf/mparse.pl --parser=swig_mapicodes --outputdir=./ $(top_builddir)/libmapi/conf/mapi-codes

.SUFFIXES: .po

.c.po:
	@echo "Compiling $< with -fPIC"
	@$(CC) $(CFLAGS) -DDEFAULT_LDIF=\"$(DESTDIR)$(datadir)/setup\" -fPIC -c $< -o $@

mapi.$(SHLIBEXT): 	mapi_wrap.po				\
			lwmapi.po				\
			../../libmapi/utf8_convert.yy.po	\
			../../libmapi/*.po			\
			../../gen_ndr/*.po			\
			../../*.po
	@echo "Compiling $<"
	@$(CC) -o $@ $(DSOOPT) $^ $(LIBS)
