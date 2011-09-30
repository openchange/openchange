#!/bin/bash

PACKAGE="posix_ipc-0.9.1"

echo '[*] Installing virtualenv'
sudo easy_install virtualenv

echo '[*] Creating the isolated python environment'
virtualenv mydevenv

echo '[*] Installing Pylons 1.0 in mydevenv'
curl http://pylonshq.com/download/1.0/go-pylons.py | python - mydevenv

echo '[*] Activating the virtual environment'
source mydevenv/bin/activate

echo '[*] Installing posix_ipc python module'
mkdir mydevenv/dist
cd mydevenv/dist
wget http://semanchuk.com/philip/posix_ipc/${PACKAGE}.tar.gz
tar xzf ${PACKAGE}.tar.gz
cd ${PACKAGE}
python setup.py install
cd ..
rm -rf ${PACKAGE}
cd ../../

echo '[*] Reconfiguring ocsmanager'
cd ocsmanager
python setup.py develop

echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
echo 'To run the ocsmanager service:'
echo 'source mydevenv/bin/activate'
echo 'cd ocsmanager'
echo 'paster serve --reload development.ini'
echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
