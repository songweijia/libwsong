#!/usr/bin/env bash
INSTALL_PREFIX=$1

git clone https://github.com/jemalloc/jemalloc.git
cd jemalloc
git checkout tags/5.3.0
./autogen.sh
./configure --prefix=$INSTALL_PREFIX --with-jemalloc-prefix=ws_ --without-export
make build_lib_static -j `nproc`
make install_lib_static
make install_lib_pc
make install_bin
make install_include
