#!/bin/bash

out_dir=./out~
www_dir=../www

pushd doc-src
echo "Update documentation source"
wget https://raw.githubusercontent.com/openchange/website/jkerihuel/mapistore-python/api/mapistore_http/Makefile -O Makefile
wget https://raw.githubusercontent.com/openchange/website/jkerihuel/mapistore-python/api/mapistore_http/source/index.rst -O source/index.rst
wget https://raw.githubusercontent.com/openchange/website/jkerihuel/mapistore-python/api/mapistore_http/source/conf.py -O source/conf.py

make singlehtml BUILDDIR=./out~
cp $out_dir/singlehtml/index.html $www_dir/
cp -r $out_dir/singlehtml/_static $www_dir/
make clean BUILDDIR=./out~
popd
