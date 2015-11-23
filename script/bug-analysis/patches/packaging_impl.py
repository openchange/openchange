'''apport.PackageInfo class implementation for python-apt and dpkg.

This is used on Debian and derivatives such as Ubuntu.
'''

# Copyright (C) 2007 - 2011 Canonical Ltd.
# Author: Martin Pitt <martin.pitt@ubuntu.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.  See http://www.gnu.org/copyleft/gpl.html for
# the full text of the license.

import subprocess, os, glob, stat, sys, tempfile, re, shutil, time
import hashlib
import requests
from urllib import urlretrieve

import warnings
warnings.filterwarnings('ignore', 'apt API not stable yet', FutureWarning)
import apt
try:
    import cPickle as pickle
    from urllib import urlopen
    (pickle, urlopen)  # pyflakes
except ImportError:
    # python 3
    from urllib.request import urlopen
    import pickle

import apport
from apport.packaging import PackageInfo


class __AptDpkgPackageInfo(PackageInfo):
    '''Concrete apport.PackageInfo class implementation for python-apt and
    dpkg, as found on Debian and derivatives such as Ubuntu.'''

    def __init__(self):
        self._apt_cache = None
        self._sandbox_apt_cache = None
        self._contents_dir = None
        self._mirror = None
        self._virtual_mapping_obj = None

        self.configuration = '/etc/default/apport'

    def __del__(self):
        try:
            if self._contents_dir:
                shutil.rmtree(self._contents_dir)
        except AttributeError:
            pass

    def _virtual_mapping(self, configdir):
        if self._virtual_mapping_obj is not None:
            return self._virtual_mapping_obj

        mapping_file = os.path.join(configdir, 'virtual_mapping.pickle')
        if os.path.exists(mapping_file):
            with open(mapping_file, 'rb') as fp:
                self._virtual_mapping_obj = pickle.load(fp)
        else:
            self._virtual_mapping_obj = {}

        return self._virtual_mapping_obj

    def _save_virtual_mapping(self, configdir):
        mapping_file = os.path.join(configdir, 'virtual_mapping.pickle')
        if self._virtual_mapping_obj is not None:
            with open(mapping_file, 'wb') as fp:
                pickle.dump(self._virtual_mapping_obj, fp)

    def _cache(self):
        '''Return apt.Cache() (initialized lazily).'''

        self._sandbox_apt_cache = None
        if not self._apt_cache:
            try:
                # avoid spewage on stdout
                progress = apt.progress.base.OpProgress()
                self._apt_cache = apt.Cache(progress, rootdir='/')
            except AttributeError:
                # older python-apt versions do not yet have above argument
                self._apt_cache = apt.Cache(rootdir='/')
        return self._apt_cache

    def _sandbox_cache(self, aptroot, apt_sources, fetchProgress):
        '''Build apt sandbox and return apt.Cache(rootdir=) (initialized lazily).

        Clear the package selection on subsequent calls.
        '''
        self._apt_cache = None
        if not self._sandbox_apt_cache:
            self._build_apt_sandbox(aptroot, apt_sources)
            rootdir = os.path.abspath(aptroot)
            self._sandbox_apt_cache = apt.Cache(rootdir=rootdir)
            try:
                # We don't need to update this multiple times.
                self._sandbox_apt_cache.update(fetchProgress)
            except apt.cache.FetchFailedException as e:
                raise SystemError(str(e))
            self._sandbox_apt_cache.open()
        else:
            self._sandbox_apt_cache.clear()
        return self._sandbox_apt_cache

    def _apt_pkg(self, package):
        '''Return apt.Cache()[package] (initialized lazily).

        Throw a ValueError if the package does not exist.
        '''
        try:
            return self._cache()[package]
        except KeyError:
            raise ValueError('package %s does not exist' % package)

    def get_version(self, package):
        '''Return the installed version of a package.'''

        pkg = self._apt_pkg(package)
        inst = pkg.installed
        if not inst:
            raise ValueError('package %s does not exist' % package)
        return inst.version

    def get_available_version(self, package):
        '''Return the latest available version of a package.'''

        return self._apt_pkg(package).candidate.version

    def get_dependencies(self, package):
        '''Return a list of packages a package depends on.'''

        cur_ver = self._apt_pkg(package)._pkg.current_ver
        if not cur_ver:
            # happens with virtual packages
            return []
        return [d[0].target_pkg.name for d in
                cur_ver.depends_list.get('Depends', []) +
                cur_ver.depends_list.get('PreDepends', []) +
                cur_ver.depends_list.get('Recommends', [])]

    def get_source(self, package):
        '''Return the source package name for a package.'''

        if self._apt_pkg(package).installed:
            return self._apt_pkg(package).installed.source_name
        elif self._apt_pkg(package).candidate:
            return self._apt_pkg(package).candidate.source_name
        else:
            raise ValueError('package %s does not exist' % package)

    def get_package_origin(self, package):
        '''Return package origin.

        Return the repository name from which a package was installed, or None
        if it cannot be determined.

        Throw ValueError if package is not installed.
        '''
        pkg = self._apt_pkg(package).installed
        if not pkg:
            raise ValueError('package is not installed')
        for origin in pkg.origins:
            if origin.origin:
                return origin.origin
        return None

    def is_distro_package(self, package):
        '''Check if a package is a genuine distro package.

        Return True for a native distro package, False if it comes from a
        third-party source.
        '''
        pkg = self._apt_pkg(package)
        # some PPA packages have installed version None, see LP#252734
        if pkg.installed and pkg.installed.version is None:
            return False

        native_origins = [self.get_os_version()[0]]
        for f in glob.glob('/etc/apport/native-origins.d/*'):
            try:
                with open(f) as fd:
                    for line in fd:
                        line = line.strip()
                        if line:
                            native_origins.append(line)
            except IOError:
                pass

        if pkg.candidate and pkg.candidate.origins:  # might be None
            for o in pkg.candidate.origins:
                if o.origin in native_origins:
                    return True
        return False

    def get_architecture(self, package):
        '''Return the architecture of a package.

        This might differ on multiarch architectures (e. g.  an i386 Firefox
        package on a x86_64 system)'''

        if self._apt_pkg(package).installed:
            return self._apt_pkg(package).installed.architecture or 'unknown'
        elif self._apt_pkg(package).candidate:
            return self._apt_pkg(package).candidate.architecture or 'unknown'
        else:
            raise ValueError('package %s does not exist' % package)

    def get_files(self, package):
        '''Return list of files shipped by a package.'''

        list = self._call_dpkg(['-L', package])
        if list is None:
            return None
        return [f for f in list.splitlines() if not f.startswith('diverted')]

    def get_modified_files(self, package):
        '''Return list of all modified files of a package.'''

        # get the maximum mtime of package files that we consider unmodified
        listfile = '/var/lib/dpkg/info/%s:%s.list' % (package, self.get_system_architecture())
        if not os.path.exists(listfile):
            listfile = '/var/lib/dpkg/info/%s.list' % package
        try:
            s = os.stat(listfile)
            if not stat.S_ISREG(s.st_mode):
                raise OSError
            max_time = max(s.st_mtime, s.st_ctime)
        except OSError:
            return []

        # create a list of files with a newer timestamp for md5sum'ing
        sums = b''
        sumfile = '/var/lib/dpkg/info/%s:%s.md5sums' % (package, self.get_system_architecture())
        if not os.path.exists(sumfile):
            sumfile = '/var/lib/dpkg/info/%s.md5sums' % package
            if not os.path.exists(sumfile):
                # some packages do not ship md5sums
                return []

        with open(sumfile, 'rb') as fd:
            for line in fd:
                try:
                    # ignore lines with NUL bytes (happens, LP#96050)
                    if b'\0' in line:
                        apport.warning('%s contains NUL character, ignoring line', sumfile)
                        continue
                    words = line.split()
                    if not words:
                        apport.warning('%s contains empty line, ignoring line', sumfile)
                        continue
                    s = os.stat(('/' + words[-1].decode('UTF-8')).encode('UTF-8'))
                    if max(s.st_mtime, s.st_ctime) <= max_time:
                        continue
                except OSError:
                    pass

                sums += line

        if sums:
            return self._check_files_md5(sums)
        else:
            return []

    def get_modified_conffiles(self, package):
        '''Return modified configuration files of a package.

        Return a file name -> file contents map of all configuration files of
        package. Please note that apport.hookutils.attach_conffiles() is the
        official user-facing API for this, which will ask for confirmation and
        allows filtering.
        '''
        dpkg = subprocess.Popen(['dpkg-query', '-W', '--showformat=${Conffiles}',
                                 package], stdout=subprocess.PIPE)

        out = dpkg.communicate()[0].decode()
        if dpkg.returncode != 0:
            return {}

        modified = {}
        for line in out.splitlines():
            if not line:
                continue
            # just take the first two fields, to not stumble over obsolete
            # conffiles
            path, default_md5sum = line.strip().split()[:2]

            if os.path.exists(path):
                try:
                    with open(path, 'rb') as fd:
                        contents = fd.read()
                    m = hashlib.md5()
                    m.update(contents)
                    calculated_md5sum = m.hexdigest()

                    if calculated_md5sum != default_md5sum:
                        modified[path] = contents
                except IOError as e:
                    modified[path] = '[inaccessible: %s]' % str(e)
            else:
                modified[path] = '[deleted]'

        return modified

    def __fgrep_files(self, pattern, file_list):
        '''Call fgrep for a pattern on given file list and return the first
        matching file, or None if no file matches.'''

        match = None
        slice_size = 100
        i = 0

        while not match and i < len(file_list):
            p = subprocess.Popen(['fgrep', '-lxm', '1', '--', pattern] +
                                 file_list[i:(i + slice_size)], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            out = p.communicate()[0].decode('UTF-8')
            if p.returncode == 0:
                match = out
            i += slice_size

        return match

    def get_file_package(self, file, uninstalled=False, map_cachedir=None,
                         release=None, arch=None):
        '''Return the package a file belongs to.

        Return None if the file is not shipped by any package.

        If uninstalled is True, this will also find files of uninstalled
        packages; this is very expensive, though, and needs network access and
        lots of CPU and I/O resources. In this case, map_cachedir can be set to
        an existing directory which will be used to permanently store the
        downloaded maps. If it is not set, a temporary directory will be used.
        Also, release and arch can be set to a foreign release/architecture
        instead of the one from the current system.
        '''
        # check if the file is a diversion
        dpkg = subprocess.Popen(['dpkg-divert', '--list', file],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = dpkg.communicate()[0].decode('UTF-8')
        if dpkg.returncode == 0 and out:
            pkg = out.split()[-1]
            if pkg != 'hardening-wrapper':
                return pkg

        fname = os.path.splitext(os.path.basename(file))[0].lower()

        all_lists = []
        likely_lists = []
        for f in glob.glob('/var/lib/dpkg/info/*.list'):
            p = os.path.splitext(os.path.basename(f))[0].lower().split(':')[0]
            if p in fname or fname in p:
                likely_lists.append(f)
            else:
                all_lists.append(f)

        # first check the likely packages
        match = self.__fgrep_files(file, likely_lists)
        if not match:
            match = self.__fgrep_files(file, all_lists)

        if match:
            return os.path.splitext(os.path.basename(match))[0].split(':')[0]

        if uninstalled:
            return self._search_contents(file, map_cachedir, release, arch)
        else:
            return None

    @classmethod
    def get_system_architecture(klass):
        '''Return the architecture of the system, in the notation used by the
        particular distribution.'''

        dpkg = subprocess.Popen(['dpkg', '--print-architecture'],
                                stdout=subprocess.PIPE)
        arch = dpkg.communicate()[0].decode().strip()
        assert dpkg.returncode == 0
        assert arch
        return arch

    def get_library_paths(self):
        '''Return a list of default library search paths.

        The entries should be separated with a colon ':', like for
        $LD_LIBRARY_PATH. This needs to take any multiarch directories into
        account.
        '''
        dpkg = subprocess.Popen(['dpkg-architecture', '-qDEB_HOST_MULTIARCH'],
                                stdout=subprocess.PIPE)
        multiarch_triple = dpkg.communicate()[0].decode().strip()
        assert dpkg.returncode == 0

        return '/lib/%s:/lib' % multiarch_triple

    def set_mirror(self, url):
        '''Explicitly set a distribution mirror URL for operations that need to
        fetch distribution files/packages from the network.

        By default, the mirror will be read from the system configuration
        files.
        '''
        self._mirror = url

        # purge our contents dir cache
        try:
            if self._contents_dir:
                shutil.rmtree(self._contents_dir)
                self._contents_dir = None
        except AttributeError:
            pass

    def get_source_tree(self, srcpackage, dir, version=None, sandbox=None,
                        apt_update=False):
        '''Download source package and unpack it into dir.

        This also has to care about applying patches etc., so that dir will
        eventually contain the actually compiled source. dir needs to exist and
        should be empty.

        If version is given, this particular version will be retrieved.
        Otherwise this will fetch the latest available version.

        If sandbox is given, it calls apt-get source in that sandbox, otherwise
        it uses the system apt configuration.

        If apt_update is True, it will call apt-get update before apt-get
        source. This is mostly necessary for freshly created sandboxes.

        Return the directory that contains the actual source root directory
        (which might be a subdirectory of dir). Return None if the source is
        not available.
        '''
        # configure apt for sandbox
        env = os.environ.copy()
        if sandbox:
            f = tempfile.NamedTemporaryFile()
            f.write(('''Dir "%s";
Debug::NoLocking "true";
 ''' % sandbox).encode())
            f.flush()
            env['APT_CONFIG'] = f.name

        if apt_update:
            subprocess.call(['apt-get', '-qq', 'update'], env=env)

        # fetch source tree
        argv = ['apt-get', '-qq', '--assume-yes', 'source', srcpackage]
        if version:
            argv[-1] += '=' + version
        try:
            if subprocess.call(argv, cwd=dir, env=env) != 0:
                return None
        except OSError:
            return None

        # find top level directory
        root = None
        for d in glob.glob(os.path.join(dir, srcpackage + '-*')):
            if os.path.isdir(d):
                root = d
        assert root, 'could not determine source tree root directory'

        # apply patches on a best-effort basis
        try:
            subprocess.call('(debian/rules patch || debian/rules apply-patches '
                            '|| debian/rules apply-dpatches || '
                            'debian/rules unpack || debian/rules patch-stamp || '
                            'debian/rules setup) >/dev/null 2>&1', shell=True, cwd=root)
        except OSError:
            pass

        return root

    def get_kernel_package(self):
        '''Return the actual Linux kernel package name.

        This is used when the user reports a bug against the "linux" package.
        '''
        # TODO: Ubuntu specific
        return 'linux-image-' + os.uname()[2]

    def _install_debug_kernel(self, report):
        '''Install kernel debug package

        Ideally this would be just another package but the kernel is
        special in various ways currently so we can not use the apt
        method.
        '''
        installed = []
        outdated = []
        kver = report['Uname'].split()[1]
        arch = report['Architecture']
        ver = report['Package'].split()[1]
        debug_pkgname = 'linux-image-debug-%s' % kver
        c = self._cache()
        if debug_pkgname in c and c[debug_pkgname].isInstalled:
            #print('kernel ddeb already installed')
            return (installed, outdated)
        target_dir = apt.apt_pkg.config.find_dir('Dir::Cache::archives') + '/partial'
        deb = '%s_%s_%s.ddeb' % (debug_pkgname, ver, arch)
        # FIXME: this package is currently not in Packages.gz
        url = 'http://ddebs.ubuntu.com/pool/main/l/linux/%s' % deb
        out = open(os.path.join(target_dir, deb), 'w')
        # urlretrieve does not return 404 in the headers so we use urlopen
        u = urlopen(url)
        if u.getcode() > 400:
            return ('', 'linux')
        while True:
            block = u.read(8 * 1024)
            if not block:
                break
            out.write(block)
        out.flush()
        ret = subprocess.call(['dpkg', '-i', os.path.join(target_dir, deb)])
        if ret == 0:
            installed.append(deb.split('_')[0])
        return (installed, outdated)

    def install_packages(self, rootdir, configdir, release, packages,
                         verbose=False, cache_dir=None,
                         permanent_rootdir=False, architecture=None):
        '''Install packages into a sandbox (for apport-retrace).

        In order to work without any special permissions and without touching
        the running system, this should only download and unpack packages into
        the given root directory, not install them into the system.

        configdir points to a directory with by-release configuration files for
        the packaging system; this is completely dependent on the backend
        implementation, the only assumption is that this looks into
        configdir/release/, so that you can use retracing for multiple
        DistroReleases. As a special case, if configdir is None, it uses the
        current system configuration, and "release" is ignored.

        release is the value of the report's 'DistroRelease' field.

        packages is a list of ('packagename', 'version') tuples. If the version
        is None, it should install the most current available version.

        If cache_dir is given, then the downloaded packages will be stored
        there, to speed up subsequent retraces.

        If permanent_rootdir is True, then the sandbox created from the
        downloaded packages will be reused, to speed up subsequent retraces.

        If architecture is given, the sandbox will be created with packages of
        the given architecture (as specified in a report's "Architecture"
        field). If not given it defaults to the host system's architecture.

        Return a string with outdated packages, or None if all packages were
        installed.

        If something is wrong with the environment (invalid configuration,
        package servers down, etc.), this should raise a SystemError with a
        meaningful error message.
        '''
        if not configdir:
            apt_sources = '/etc/apt/sources.list'
            self.current_release_codename = self.get_distro_codename()
        else:
            # support architecture specific config, fall back to global config
            apt_sources = os.path.join(configdir, release, 'sources.list')
            if architecture:
                arch_apt_sources = os.path.join(configdir, release,
                                                architecture, 'sources.list')
                if os.path.exists(arch_apt_sources):
                    apt_sources = arch_apt_sources

            # set mirror for get_file_package()
            try:
                self.set_mirror(self._get_primary_mirror_from_apt_sources(apt_sources))
            except SystemError as e:
                apport.warning('cannot determine mirror: %s' % str(e))

            # set current release code name for _distro_release_to_codename
            with open(os.path.join(configdir, release, 'codename')) as f:
                self.current_release_codename = f.read().strip()

        if not os.path.exists(apt_sources):
            raise SystemError('%s does not exist' % apt_sources)

        # create apt sandbox
        if cache_dir:
            tmp_aptroot = False
            if configdir:
                aptroot = os.path.join(cache_dir, release, 'apt')
            else:
                aptroot = os.path.join(cache_dir, 'system', 'apt')
            if not os.path.isdir(aptroot):
                os.makedirs(aptroot)
        else:
            tmp_aptroot = True
            aptroot = tempfile.mkdtemp()

        if architecture:
            apt.apt_pkg.config.set('APT::Architecture', architecture)
        else:
            apt.apt_pkg.config.set('APT::Architecture', self.get_system_architecture())

        if verbose:
            fetchProgress = apt.progress.text.AcquireProgress()
        else:
            fetchProgress = apt.progress.base.AcquireProgress()
        if not tmp_aptroot:
            c = self._sandbox_cache(aptroot, apt_sources, fetchProgress)
        else:
            self._build_apt_sandbox(aptroot, apt_sources)
            c = apt.Cache(rootdir=os.path.abspath(aptroot))
            try:
                c.update(fetchProgress)
            except apt.cache.FetchFailedException as e:
                raise SystemError(str(e))
            c.open()

        obsolete = ''

        src_records = apt.apt_pkg.SourceRecords()

        REPO = 'http://archive.zentyal.org/old'
        UBUNTU_REPO = 'http://archive.ubuntu.com/ubuntu'
        DBG_PKGS = {'libldb1': 'ldb',
                    'libsope1': 'sope',
                    'libc6': 'eglibc',
                    'libwbclient0': 'samba',
                    'libwbxml2-0': 'wbxml2',
                    'openchangeserver': 'openchange',
                    'openchangeproxy': 'openchange',
                    'libmapi0': 'openchange',
                    'libmapistore0': 'openchange',
                    'libmapiproxy0': 'openchange',
                    'libnss-winbind': 'samba',
                    'libpam-winbind': 'samba',
                    'python-ldb': 'ldb',
                    'python-talloc': 'talloc',
                    'samba': 'samba',
                    'samba-common-bin': 'samba',
                    'samba-dsdb-modules': 'samba',
                    'samba-libs': 'samba',
                    'samba-testsuite': 'samba',
                    'samba-vfs-modules': 'samba',
                    'sogo': 'sogo',
                    'sogo-openchange': 'sogo',
                    'libtalloc2': 'talloc',
                    'libtevent0': 'tevent',
                    'libntdb1': 'ntdb',
                    'winbind': 'samba'}

        # urls to overwrite dbg packages with proper version
        dbg_pkg_urls = {}
        pkgs_not_found = []

        # mark packages for installation
        real_pkgs = set()
        for (pkg, ver) in packages:
            if not ver: continue
            if pkg in DBG_PKGS.keys():
                source = DBG_PKGS[pkg]
                for pname in [pkg, pkg + '-dbg']:
                    for repo in (REPO, UBUNTU_REPO):
                        filename = pname + '_' + re.sub('^.*:', '', ver) + '_amd64.deb'
                        url = repo + '/pool/main/' + source[0:1] + '/' + source + '/' + filename
                        if requests.head(url).status_code == 200:
                            dbg_pkg_urls[pname] = url
                            break
                        elif repo == UBUNTU_REPO and pname[-4:] != '-dbg':
                            pkgs_not_found.append(filename)

            try:
                candidate = c[pkg].candidate
            except KeyError:
                candidate = None
            if not candidate:
                m = 'package %s does not exist, ignoring' % pkg.replace('%', '%%')
                obsolete += m + '\n'
                apport.warning(m)
                continue

            if ver and candidate.version != ver:
                w = '%s version %s required, but %s is available' % (pkg, ver, candidate.version)
                obsolete += w + '\n'
            real_pkgs.add(pkg)

            if permanent_rootdir:
                virtual_mapping = self._virtual_mapping(aptroot)
                # Remember all the virtual packages that this package provides,
                # so that if we encounter that virtual package as a
                # Conflicts/Replaces later, we know to remove this package from
                # the cache.
                for p in candidate.provides:
                    virtual_mapping.setdefault(p, set()).add(pkg)
                conflicts = []
                if 'Conflicts' in candidate.record:
                    conflicts += apt.apt_pkg.parse_depends(candidate.record['Conflicts'])
                if 'Replaces' in candidate.record:
                    conflicts += apt.apt_pkg.parse_depends(candidate.record['Replaces'])
                archives = apt.apt_pkg.config.find_dir('Dir::Cache::archives')
                for conflict in conflicts:
                    # apt_pkg.parse_depends needs to handle the or operator,
                    # but as policy states it is invalid to use that in
                    # Replaces/Depends, we can safely choose the first value
                    # here.
                    conflict = conflict[0]
                    if c.is_virtual_package(conflict[0]):
                        try:
                            providers = virtual_mapping[conflict[0]]
                        except KeyError:
                            # We may not have seen the virtual package that
                            # this conflicts with, so we can assume it's not
                            # unpacked into the sandbox.
                            continue
                        for p in providers:
                            debs = os.path.join(archives, '%s_*.deb' % p)
                            for path in glob.glob(debs):
                                ver = self._deb_version(path)
                                if apt.apt_pkg.check_dep(ver, conflict[2], conflict[1]):
                                    os.unlink(path)
                        del providers
                    else:
                        debs = os.path.join(archives, '%s_*.deb' % conflict[0])
                        for path in glob.glob(debs):
                            ver = self._deb_version(path)
                            if apt.apt_pkg.check_dep(ver, conflict[2], conflict[1]):
                                os.unlink(path)

            if candidate.architecture != 'all':
                if pkg + '-dbg' in c:
                    real_pkgs.add(pkg + '-dbg')
                else:
                    # install all -dbg from the source package
                    if src_records.lookup(candidate.source_name):
                        dbgs = [p for p in src_records.binaries if p.endswith('-dbg') and p in c]
                    else:
                        dbgs = []
                    if dbgs:
                        for p in dbgs:
                            real_pkgs.add(p)
                    else:
                        if pkg + '-dbgsym' in c:
                            real_pkgs.add(pkg + '-dbgsym')
                            if c[pkg + '-dbgsym'].candidate.version != candidate.version:
                                obsolete += 'outdated debug symbol package for %s: package version %s dbgsym version %s\n' % (
                                    pkg, candidate.version, c[pkg + '-dbgsym'].candidate.version)

        if pkgs_not_found:
            print "Aborting retrace as some packages cannot be found in the repos:"
            print "\n".join(pkgs_not_found)
            sys.exit(1)

        for p in real_pkgs:
            c[p].mark_install(False, False)

        last_written = time.time()
        # fetch packages
        fetcher = apt.apt_pkg.Acquire(fetchProgress)
        try:
            c.fetch_archives(fetcher=fetcher)
        except apt.cache.FetchFailedException as e:
            apport.error('Package download error, try again later: %s', str(e))
            sys.exit(99)  # transient error

        # FIXME: unhardcode path
        pkgs_path = cache_dir + '/Ubuntu 14.04/apt/var/cache/apt/archives'
        debs_to_unpack = []
        for pname, url in dbg_pkg_urls.iteritems():
            # Remove other versions before downloading the proper one
            for i in glob.glob(os.path.join(pkgs_path, '%s_*.deb' % pname)):
                #print "DELETING AUTO-FETCHED DEB: " + i
                os.unlink(i)
            #print "DOWNLOADING URL: " + url + "\n";
            urlretrieve(url, pkgs_path + '/' + os.path.basename(url))
            debs_to_unpack.append(pkgs_path + '/' + os.path.basename(url))

        # unpack packages
        if verbose:
            print('Extracting downloaded debs...')
        for i in fetcher.items:
            if not permanent_rootdir or os.path.getctime(i.destfile) > last_written:
                if os.path.isfile(i.destfile):
                    subprocess.check_call(['dpkg', '-x', i.destfile, rootdir])
            real_pkgs.remove(os.path.basename(i.destfile).split('_', 1)[0])

        for debfile in debs_to_unpack:
            #print "UNPACKING NEW DOWNLOADED DEB: " + debfile + " IN: " + rootdir
            subprocess.check_call(['dpkg', '-x', debfile, rootdir])

        if tmp_aptroot:
            shutil.rmtree(aptroot)

        # check bookkeeping that apt fetcher really got everything
        assert not real_pkgs, 'apt fetcher did not fetch these packages: ' \
            + ' '.join(real_pkgs)

        if permanent_rootdir:
            self._save_virtual_mapping(aptroot)

        return obsolete

    def package_name_glob(self, nameglob):
        '''Return known package names which match given glob.'''

        return glob.fnmatch.filter(self._cache().keys(), nameglob)

    #
    # Internal helper methods
    #

    @classmethod
    def _call_dpkg(klass, args):
        '''Call dpkg with given arguments and return output, or return None on
        error.'''

        dpkg = subprocess.Popen(['dpkg'] + args, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        out = dpkg.communicate(input)[0].decode('UTF-8')
        if dpkg.returncode == 0:
            return out
        else:
            raise ValueError('package does not exist')

    def _check_files_md5(self, sumfile):
        '''Internal function for calling md5sum.

        This is separate from get_modified_files so that it is automatically
        testable.
        '''
        if os.path.exists(sumfile):
            m = subprocess.Popen(['/usr/bin/md5sum', '-c', sumfile],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 cwd='/', env={})
            out = m.communicate()[0].decode('UTF-8', errors='replace')
        else:
            assert type(sumfile) == bytes, 'md5sum list value must be a byte array'
            m = subprocess.Popen(['/usr/bin/md5sum', '-c'],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, cwd='/', env={})
            out = m.communicate(sumfile)[0].decode('UTF-8', errors='replace')

        # if md5sum succeeded, don't bother parsing the output
        if m.returncode == 0:
            return []

        mismatches = []
        for l in out.splitlines():
            if l.endswith('FAILED'):
                mismatches.append(l.rsplit(':', 1)[0])

        return mismatches

    @classmethod
    def _get_primary_mirror_from_apt_sources(klass, apt_sources):
        '''Heuristically determine primary mirror from an apt sources.list'''

        with open(apt_sources) as f:
            for l in f:
                fields = l.split()
                if len(fields) >= 3 and fields[0] == 'deb':
                    if fields[1].startswith('['):
                        # options given, mirror is in third field
                        mirror_idx = 2
                    else:
                        mirror_idx = 1
                    if fields[mirror_idx].startswith('http://'):
                        return fields[mirror_idx]
            else:
                raise SystemError('cannot determine default mirror: %s does not contain a valid deb line'
                                  % apt_sources)

    def _get_mirror(self):
        '''Return the distribution mirror URL.

        If it has not been set yet, it will be read from the system
        configuration.'''

        if not self._mirror:
            self._mirror = self._get_primary_mirror_from_apt_sources('/etc/apt/sources.list')
        return self._mirror

    def _distro_release_to_codename(self, release):
        '''Map a DistroRelease: field value to a release code name'''

        # if we called install_packages() with a configdir, we can read the
        # codename from there
        if hasattr(self, 'current_release_codename') and self.current_release_codename is not None:
            return self.current_release_codename

        raise NotImplementedError('Cannot map DistroRelease to a code name without install_packages()')

    def _search_contents(self, file, map_cachedir, release, arch):
        '''Internal function for searching file in Contents.gz.'''

        if map_cachedir:
            dir = map_cachedir
        else:
            if not self._contents_dir:
                self._contents_dir = tempfile.mkdtemp()
            dir = self._contents_dir

        if arch is None:
            arch = self.get_system_architecture()
        if release is None:
            release = self.get_distro_codename()
        else:
            release = self._distro_release_to_codename(release)
        for pocket in ['-updates', '-security', '-proposed', '']:
            map = os.path.join(dir, '%s%s-Contents-%s.gz' % (release, pocket, arch))

            # check if map exists and is younger than a day; if not, we need to
            # refresh it
            try:
                st = os.stat(map)
                age = int(time.time() - st.st_mtime)
            except OSError:
                age = None

            if age is None or age >= 86400:
                url = '%s/dists/%s%s/Contents-%s.gz' % (self._get_mirror(), release, pocket, arch)

                try:
                    src = urlopen(url)
                except IOError:
                    # we ignore non-existing pockets, but we do crash if the
                    # release pocket doesn't exist
                    if pocket == '':
                        raise
                    else:
                        continue

                with open(map, 'wb') as f:
                    while True:
                        data = src.read(1000000)
                        if not data:
                            break
                        f.write(data)
                src.close()
                assert os.path.exists(map)

            if file.startswith('/'):
                file = file[1:]

            # zgrep is magnitudes faster than a 'gzip.open/split() loop'
            package = None
            zgrep = subprocess.Popen(['zgrep', '-m1', '^%s[[:space:]]' % file, map],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            out = zgrep.communicate()[0].decode('UTF-8')
            # we do not check the return code, since zgrep -m1 often errors out
            # with 'stdout: broken pipe'
            if out:
                package = out.split()[1].split(',')[0].split('/')[-1]
            if package:
                return package
        return None

    @classmethod
    def _build_apt_sandbox(klass, apt_root, apt_sources):
        # pre-create directories, to avoid apt.Cache() printing "creating..."
        # messages on stdout
        if not os.path.exists(os.path.join(apt_root, 'var', 'lib', 'apt')):
            os.makedirs(os.path.join(apt_root, 'var', 'lib', 'apt', 'lists', 'partial'))
            os.makedirs(os.path.join(apt_root, 'var', 'cache', 'apt', 'archives', 'partial'))
            os.makedirs(os.path.join(apt_root, 'var', 'lib', 'dpkg'))
            os.makedirs(os.path.join(apt_root, 'etc', 'apt', 'apt.conf.d'))
            os.makedirs(os.path.join(apt_root, 'etc', 'apt', 'preferences.d'))

        # install apt sources
        list_d = os.path.join(apt_root, 'etc', 'apt', 'sources.list.d')
        if os.path.exists(list_d):
            shutil.rmtree(list_d)
        if os.path.isdir(apt_sources + '.d'):
            shutil.copytree(apt_sources + '.d', list_d)
        else:
            os.makedirs(list_d)
        with open(apt_sources) as src:
            with open(os.path.join(apt_root, 'etc', 'apt', 'sources.list'), 'w') as dest:
                dest.write(src.read())

        # install apt keyrings; prefer the ones from the config dir, fall back
        # to system
        trusted_gpg = os.path.join(os.path.dirname(apt_sources), 'trusted.gpg')
        if os.path.exists(trusted_gpg):
            shutil.copy(trusted_gpg, os.path.join(apt_root, 'etc', 'apt'))
        elif os.path.exists('/etc/apt/trusted.gpg'):
            shutil.copy('/etc/apt/trusted.gpg', os.path.join(apt_root, 'etc', 'apt'))

        trusted_d = os.path.join(apt_root, 'etc', 'apt', 'trusted.gpg.d')
        if os.path.exists(trusted_d):
            shutil.rmtree(trusted_d)

        if os.path.exists(trusted_gpg + '.d'):
            shutil.copytree(trusted_gpg + '.d', trusted_d)
        elif os.path.exists('/etc/apt/trusted.gpg.d'):
            shutil.copytree('/etc/apt/trusted.gpg.d', trusted_d)
        else:
            os.makedirs(trusted_d)

    @classmethod
    def _deb_version(klass, pkg):
        '''Return the version of a .deb file'''

        dpkg = subprocess.Popen(['dpkg-deb', '-f', pkg, 'Version'], stdout=subprocess.PIPE)
        out = dpkg.communicate(input)[0].decode('UTF-8')
        assert dpkg.returncode == 0
        assert out
        return out

    def compare_versions(self, ver1, ver2):
        '''Compare two package versions.

        Return -1 for ver < ver2, 0 for ver1 == ver2, and 1 for ver1 > ver2.'''

        return apt.apt_pkg.version_compare(ver1, ver2)

    def enabled(self):
        '''Return whether Apport should generate crash reports.

        Signal crashes are controlled by /proc/sys/kernel/core_pattern, but
        some init script needs to set that value based on a configuration file.
        This also determines whether Apport generates reports for Python,
        package, or kernel crashes.

        Implementations should parse the configuration file which controls
        Apport (such as /etc/default/apport in Debian/Ubuntu).
        '''

        try:
            with open(self.configuration) as f:
                conf = f.read()
        except IOError:
            # if the file does not exist, assume it's enabled
            return True

        return re.search('^\s*enabled\s*=\s*0\s*$', conf, re.M) is None

    _distro_codename = None

    def get_distro_codename(self):
        '''Get "lsb_release -sc", cache the result.'''

        if self._distro_codename is None:
            lsb_release = subprocess.Popen(['lsb_release', '-sc'],
                                           stdout=subprocess.PIPE)
            self._distro_codename = lsb_release.communicate()[0].decode('UTF-8').strip()
            assert lsb_release.returncode == 0

        return self._distro_codename

impl = __AptDpkgPackageInfo()
