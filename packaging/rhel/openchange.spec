%{!?python_sys_pyver: %global python_sys_pyver %(/usr/bin/python -c "import sys; print sys.hexversion")}

# if hex(sys.hexversion) < 0x02060000
%if %{python_sys_pyver} < 33947648
%global __python /usr/bin/python2.6
%endif

%{!?python_noarch_sitearch: %global python_noarch_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(0)")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

%global samba4_version 4.0.0alpha18-2
%global talloc_version 1.2.0
%global python_config python2.6-config

### Abstract ###

# Licensing Note: The code is GPLv3+ and the IDL files are public domain.

Name: openchange
Version: 1.0.0%{?build_suffix}.sogo
Release: 1%{?dist}
Group: Applications/System
Summary: Provides access to Microsoft Exchange servers using native protocols
License: GPLv3+ and Public Domain
URL: http://www.openchange.org/
Source0: http://downloads.sourceforge.net/openchange/openchange-%{version}.tar.gz
Source1: doxygen_to_devhelp.xsl
Source2: openchange-ocsmanager.init
Patch0: openchange-1.0-no_ocpf.diff
Patch1: ocsmanager-ini-path-fix.diff
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
AutoReqProv: 0
Requires: samba4 >= %{samba4_version}
### Build Dependencies ###

BuildRequires: bison
BuildRequires: doxygen
BuildRequires: file
# BuildRequires: flex
BuildRequires: pkgconfig
BuildRequires: libxslt
BuildRequires: popt
BuildRequires: samba4 >= %{samba4_version}
BuildRequires: sqlite-devel
BuildRequires: zlib-devel
BuildRequires: automake
BuildRequires: autoconf
%if %{python_sys_pyver} < 33947648
BuildRequires: python26-devel
BuildRequires: python26-setuptools >= 0.6c9
BuildRequires: python26-paste-script
%else
BuildRequires: python-devel
BuildRequires: python-setuptools >= 0.6c9
BuildRequires: python-paste-script
%endif
%if 0%{?rhel} >= 6
BuildRequires: popt-devel
%endif

%description
OpenChange provides libraries to access Microsoft Exchange servers
using native protocols.
Requires: devhelp

%package ocsmanager
Group: Applications/System
Summary: OpenChange - web services
Requires: openchange = %{version}-%{release}
%if %{python_sys_pyver} < 33947648
Requires: python26
Requires: python26-pylons
Requires: python26-rpclib
%else
Requires: python
Requires: python-pylons
Requires: python-rpclib
%endif

%description ocsmanager
This packages provides web services for OpenChange in the form of a Pylons
application.

%package rpcproxy
Group: Applications/System
Summary: OpenChange - RPC-over-HTTP proxy
Requires: openchange = %{version}-%{release}
%if %{python_sys_pyver} < 33947648
Requires: python26
Requires: python26-mod_wsgi
%else
Requires: python
Requires: mod_wsgi
%endif

%description rpcproxy
This package contains a a RPC-over-HTTP python implementation
for Samba, using wsgi.

# %package devel
# Summary: Developer tools for OpenChange libraries
# Group: Development/Libraries
# Requires: openchange = %{version}-%{release}

# %description devel
# This package provides the development tools and headers for
# OpenChange, providing libraries to access Microsoft Exchange servers
# using native protocols.

# %package devel-docs
# Summary: Developer documentation for OpenChange libraries
# Group: Development/Libraries
# Requires: openchange = %{version}-%{release}

# %description devel-docs
# This package contains developer documentation for Openchange.

# %package client
# Summary: User tools for OpenChange libraries
# Group: Applications/System
# Requires: openchange = %{version}-%{release}

# %description client
# This package provides the user tools for OpenChange, providing access to
# Microsoft Exchange servers using native protocols.

# %if %{build_python_package}
# %package python
# Summary: Python bindings for OpenChange libraries
# Group: Development/Libraries
# Requires: openchange = %{version}-%{release}

# %description python
# This module contains a wrapper that allows the use of OpenChange via Python.
# %endif

# %if %{build_server_package}
# %package server
# Summary: Server-side modules for OpenChange
# Group: Applications/System
# Requires: openchange = %{version}-%{release}
# Requires: sqlite

# %description server
# This package provides the server elements for OpenChange.
# %endif

%prep
%setup -q -n %{name}-%{version}
%patch0 -p1
%patch1 -p0

%build
./autogen.sh
mkdir rpmbin
ln -s %{__python} rpmbin/python
ln -s /usr/bin/%{python_config} rpmbin/python-config
export PATH=$PWD/rpmbin:$PATH

CFLAGS="-O2 -ggdb" \
PYTHON=%{__python} \
PYTHON_CONFIG="/bin/false" \
%configure --with-modulesdir=%{_libdir}/samba4/modules --datadir=%{_datadir}/samba PYTHON=%{__python}

# Parallel builds prohibited by makefile
make
make doxygen
for x in openchange_newuser openchange_provision; do \
  sed -e 's@/bin/python$@/bin/python2.6@g'  setup/$x > setup/$x.sed; \
  mv -f setup/$x.sed setup/$x; \
  chmod 755 setup/$x; \
done


%install
rm -rf $RPM_BUILD_ROOTS

make install DESTDIR=$RPM_BUILD_ROOT SERVER_MODULESDIR=%{_libdir}/samba4/modules/dcerpc_server
cp -r libmapi++ $RPM_BUILD_ROOT%{_includedir}

# This makes the right links, as rpmlint requires that the
# ldconfig-created links be recorded in the RPM.
/sbin/ldconfig -N -n $RPM_BUILD_ROOT/%{_libdir}

mkdir $RPM_BUILD_ROOT%{_mandir}
cp -r doc/man/man1 $RPM_BUILD_ROOT%{_mandir}
cp -r apidocs/man/man3 $RPM_BUILD_ROOT%{_mandir}

# Avoid a file conflict with man-pages package.
# Page is still reachable as "mapi_obj_bookmark".
rm -rf $RPM_BUILD_ROOT%{_mandir}/man3/index.3

install -d $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapi
cp -r apidocs/html/libmapi/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapi

install -d $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapiadmin
cp -r apidocs/html/libmapiadmin/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapiadmin

# mkdir -p $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libocpf
# cp -r apidocs/html/libocpf/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libocpf

install -d $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-mapitest
cp -r apidocs/html/mapitest/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-mapitest

install -d $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-mapiproxy
cp -r apidocs/html/mapiproxy/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-mapiproxy

install -d $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapi++
cp -r apidocs/html/libmapi++/* $RPM_BUILD_ROOT%{_datadir}/devhelp/books/openchange-libmapi++

# OCSMANAGER
install -d $RPM_BUILD_ROOT/etc/ocsmanager
install -m 644 mapiproxy/services/ocsmanager/ocsmanager.ini $RPM_BUILD_ROOT/etc/ocsmanager

install -d $RPM_BUILD_ROOT/etc/httpd/conf.d/
install -m 644 mapiproxy/services/ocsmanager/ocsmanager-apache.conf $RPM_BUILD_ROOT/etc/httpd/conf.d/ocsmanager.conf

install -d $RPM_BUILD_ROOT/etc/init.d
install -m 755 %{_sourcedir}/openchange-ocsmanager.init $RPM_BUILD_ROOT/etc/init.d/openchange-ocsmanager

install -d -m 750 $RPM_BUILD_ROOT/var/log/ocsmanager

# used by ocsmanager and rpcproxy
install -d -m 700 -o apache -g apache $RPM_BUILD_ROOT/var/cache/ntlmauthhandler

(cd mapiproxy/services/ocsmanager; %{__python} setup.py install --root=$RPM_BUILD_ROOT --prefix=/usr)

# RPCPROXY
install -m 0644 mapiproxy/services/web/rpcproxy/rpcproxy.conf $RPM_BUILD_ROOT/etc/httpd/conf.d/rpcproxy.conf
install -d $RPM_BUILD_ROOT/usr/lib/openchange/web/rpcproxy
install -m 0644 mapiproxy/services/web/rpcproxy/rpcproxy.wsgi $RPM_BUILD_ROOT/usr/lib/openchange/web/rpcproxy/rpcproxy.wsgi
(cd mapiproxy/services/web/rpcproxy; python$* setup.py install --install-lib=/usr/lib/openchange/web/rpcproxy --root $RPM_BUILD_ROOT \
  --install-scripts=/usr/lib/openchange/web/rpcproxy)

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

# %if %{build_server_package}
# %post server -p /sbin/ldconfig

# %postun server -p /sbin/ldconfig
# %endif

%files
%defattr(-,root,root,-)
%doc ChangeLog COPYING IDL_LICENSE.txt VERSION

/var/cache/ntlmauthhandler

# %{_libdir}/libmapi-openchange.so.*
%{_libdir}/libmapiadmin.so.*
%{_libdir}/libmapi.so.*
%{_libdir}/libmapiproxy.so.*
# %{_libdir}/libocpf.so.*

# %files devel
# %defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*
%{_mandir}/man3/*
%doc apidocs/html/libmapi
# %doc apidocs/html/libocpf
%doc apidocs/html/overview
%doc apidocs/html/index.html

# %files devel-docs
# %defattr(-,root,root,-)
%doc %{_datadir}/devhelp/books/*

# %files client
# %defattr(-,root,root,-)
%{_bindir}/*
%{_mandir}/man1/*

# %if %{build_python_package}
# %files python
# %defattr(-,root,root,-)
%{python_sitearch}/openchange
# %endif

# %if %{build_server_package}
# %files server
# %defattr(-,root,root,-)
%{_sbindir}/*
%{_libdir}/libmapiserver.so.*
%{_libdir}/libmapistore.so.*
# %{_libdir}/mapistore_backends/mapistore_sqlite3.so
%{_libdir}/samba4/modules/*/*
%{_libdir}/nagios/check_exchange
%{_datadir}/samba/*
# %endif

%files ocsmanager
%config(noreplace) /etc/init.d/openchange-ocsmanager
%config(noreplace) /etc/ocsmanager/*
%config(noreplace) /etc/httpd/conf.d/ocsmanager.conf
%{python_noarch_sitearch}/ocsmanager*
/var/log/ocsmanager

%files rpcproxy
%config(noreplace)  /etc/httpd/conf.d/rpcproxy.conf
/usr/lib/openchange/web/rpcproxy/*
/usr/lib/openchange/web/rpcproxy/rpcproxy/*

%changelog
* Wed Aug 9 2012 Jean Raby <jraby@inverse.ca> 1.0.prerelease
- add missing log folder for ocsmanager

* Wed Aug 1 2012 Jean Raby <jraby@inverse.ca> 1.0.prerelease
- split openchange, ocsmanager and rpcproxy
- use install -d instead of mkdir, for consistency
- add rpcproxy

* Tue Jul 31 2012 Jean Raby <jraby@inverse.ca> 1.0.prerelease
- use ocsmanager.ini from the distribution
- install the apache config now shipped with the distribution
- create /var/lib/ntlmauthhandler. needed by ocsmanager
- use config(noreplace) for ocsmanager config files

* Wed Oct 12 2011 Wolfgang Sourdeau <wsourdeau@inverse.ca> 4.0.0-25.alpha17.1
- Update package to openchange-sogo 0.11.20111210
- Build everything unconditionnally and in the same package
- Removed auto dependencies

* Tue Apr 26 2011 Milan Crha <mcrha@redhat.com> - 0.9-16
- Add patch for Gnome bug #632784 (send to recipients with Unicode letters in a name)

* Thu Mar 10 2011 Milan Crha <mcrha@redhat.com> - 0.9-15
- Rebuild against newer libldb (previously was picked old version)

* Mon Feb 21 2011 Milan Crha <mcrha@redhat.com> - 0.9-14
- Rebuild against newer libldb

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9-13
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Wed Feb 02 2011 Milan Crha <mcrha@redhat.com> - 0.9-12
- Add patch for message sending, found by Gianluca Cecchi (RH bug #674034)

* Thu Dec 16 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-11
- Re-fix man-pages file conflict (RH bug #654729).

* Thu Dec 02 2010 Milan Crha <mcrha@redhat.com> - 0.9-10
- Add patch for talloc abort in FindGoodServer, found by Jeff Raber (RH bug #602661)

* Wed Sep 29 2010 jkeating - 0.9-9
- Rebuilt for gcc bug 634757

* Mon Sep 13 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-8
- Backport unicode and properties support (RH bug #605364).

* Wed Jul 21 2010 David Malcolm <dmalcolm@redhat.com> - 0.9-7
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Thu Jun 24 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-6
- Disable python and server subpackages until they're needed.

* Wed Jun 23 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-5
- More spec file cleanups.

* Fri Jun 18 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-4
- Spec file cleanups.

* Mon May 24 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-3
- Avoid a file conflict with man-pages package.

* Sat Jan 09 2010 Matthew Barnes <mbarnes@redhat.com> - 0.9-2
- Add a devel-docs subpackage (RH bug #552984).

* Sat Dec 26 2009 Matthew Barnes <mbarnes@redhat.com> - 0.9-1
- Update to 0.9 (COCHRANE)
- Bump samba4 requirement to alpha10.

* Wed Sep 23 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8.2-5
- Rebuild.

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.8.2-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Mon Jun 29 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8.2-3
- Rename libmapi so as not to conflict with Zarafa (RH bug #505783).

* Thu May 07 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8.2-2
- Do not own the pkgconfig directory (RH bug #499655).

* Tue Mar 31 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8.2-1
- Update to 0.8.2
- Add a server subpackage.
- Add BuildRequires: sqlite-devel (for server)

* Sun Mar 08 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-6
- Fix build breakage.
- Explicitly require libldb-devel.
- Bump samba4 requirement to alpha7.

* Wed Feb 25 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-5
- Rebuild with correct tarball.

* Wed Feb 25 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-4
- Formal package review cleanups.

* Wed Feb 25 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-3
- Add documentation files.

* Thu Feb 19 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-2
- Add some missing build requirements.

* Thu Jan 20 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-1
- Update to 0.8 (ROMULUS)

* Sat Jan 17 2009 Matthew Barnes <mbarnes@redhat.com> - 0.8-0.7.svn949
- Add missing BuildRequires: zlib-devel

* Sat Dec 27 2008 Matthew Barnes <mbarnes@redhat.com> - 0.8-0.6.svn949
- Update to SVN revision 949.

* Mon Dec 15 2008 Matthew Barnes <mbarnes@redhat.com> - 0.8-0.5.svn909
- Package review feedback (RH bug #453395).

* Fri Dec 12 2008 Matthew Barnes <mbarnes@redhat.com> - 0.8-0.4.svn909
- Update to SVN revision 909.
- Bump the samba4 requirement.

* Fri Aug 29 2008 Andrew Bartlett <abartlet@samba.org> - 0:0.8-0.3.svn960.fc9
- Bump version
- Don't make the Samba4 version distro-dependent

* Sat Jul 26 2008 Brad Hards <bradh@frogmouth.net> - 0:0.8-0.2.svnr674.fc10
- Bump version
- Install documentation / man pages correctly
- Remove epoch (per https://bugzilla.redhat.com/show_bug.cgi?id=453395)
- Remove %%post and %%postun (per https://bugzilla.redhat.com/show_bug.cgi?id=453395)
- Remove talloc dependency (per https://bugzilla.redhat.com/show_bug.cgi?id=453395)
- Take out libmapiproxy, because we aren't up to server side yet.

* Sat Jul 12 2008 Andrew Bartlett <abartlet@samba.org> - 0:0.7-0.2.svnr627.fc9
- Add popt-devel BR

* Mon Jun 30 2008 Andrew Bartlett <abartlet@samba.org> - 0:0.7-0.1.svnr627.fc9
- Initial package of OpenChange for Fedora
