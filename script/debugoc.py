#!/usr/bin/python
#
# OpenChange debugging script
#
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2014
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
import sys
import signal
import subprocess
import logging

LOG_LEVEL = logging.DEBUG

def setLogger():
	logger = logging.getLogger()
	logger.setLevel(logging.INFO)

	formatter = logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s')
	lsh = logging.StreamHandler()
	lsh.setLevel(LOG_LEVEL)
	lsh.setFormatter(formatter)
	logger.addHandler(lsh)
	return logger


def getSambaProcess(logger):
	logger.info("Retrieving the list of Samba process")
        ps = subprocess.Popen("ps -U 0", shell=True, stdout=subprocess.PIPE)
        psbuffer = ps.stdout.read()
        ps.stdout.close()
        ps.wait()
	return psbuffer	


def runGdb(pid, logger):
	signal.signal(signal.SIGINT, signal.SIG_IGN)
	try:
		cmd = ['gdb', '--pid', pid]
		for arg in sys.argv[1:]:
		    cmd.append('--directory=' + arg)
		subprocess.call(cmd)
	except OSError as e:
		if e.errno == os.errno.ENOENT:
			logger.critical('GDB not found in path')
			return False
		else:
			raise
	return True


def main():
	logger = setLogger()
	psbuffer = getSambaProcess(logger)
	if not 'samba' in psbuffer:
		logger.info('Samba is not running')
		return
		
	for line in psbuffer.split('\n'):
		if not 'samba' in line: continue
		logger.info(line)
		pid = filter(None, line.split(' '))[0]
		fh = open('/proc/%s/maps' % pid, 'r')
		try:
			map = fh.read()
			if 'dcerpc_mapiproxy' in map: return runGdb(pid, logger)
		except (IOError, OSError) as e:
			logger.error(e)
			return	
		finally:
			fh.close()

if __name__ == '__main__':
    main()
