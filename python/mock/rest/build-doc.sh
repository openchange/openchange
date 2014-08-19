#!/bin/bash

out_dir=./out~
www_dir=../www

pushd doc-src
make singlehtml BUILDDIR=./out~
cp $out_dir/singlehtml/index.html $www_dir/
cp -r $out_dir/singlehtml/_static $www_dir/
make clean BUILDDIR=./out~
popd
