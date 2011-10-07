#!/bin/bash

echo '[*] Installing virtualenv'
sudo easy_install virtualenv

echo '[*] Creating the isolated python environment'
virtualenv mydevenv

echo '[*] Installing Pylons 1.0 in mydevenv'
curl http://pylonshq.com/download/1.0/go-pylons.py | python - mydevenv

echo '[*] Activating the virtual environment'
source mydevenv/bin/activate

echo '[*] Reconfiguring ocsmanager'
cd ocsmanager
python setup.py develop

echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
echo 'To run the ocsmanager service:'
echo 'source mydevenv/bin/activate'
echo 'cd ocsmanager'
echo 'paster serve --reload development.ini'
echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
