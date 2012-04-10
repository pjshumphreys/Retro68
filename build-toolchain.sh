SRC=$(cd `dirname $0` && pwd -P)
mkdir -p binutils-build
mkdir -p toolchain
PREFIX=`pwd`/toolchain/
set -e

cd binutils-build
export "CFLAGS=-Wno-error"
$SRC/binutils/configure --target=m68k-unknown-elf --prefix=$PREFIX
make -j8
make install

cd ..

mkdir -p gcc-build
cd gcc-build
$SRC/gcc/configure --target=m68k-unknown-elf --prefix=$PREFIX --enable-languages=c,c++ --with-arch=m68k --with-cpu=m68000 --disable-libssp
make -j8
make install

cd ..

BINUTILS=$(cd binutils-build && pwd -P)

cp $SRC/elf.h $PREFIX/include/
export "CFLAGS=-I../../Retro68/binutils/include -I../toolchain/include"
mkdir -p elf2flt-build
cd elf2flt-build
$SRC/elf2flt/configure --target=m68k-unknown-elf --prefix=$PREFIX --with-binutils-build-dir=$BINUTILS
make -j8 TOOLDIR=$PREFIX/bin
make install
unset CFLAGS

cd ..

mkdir -p $PREFIX/man/man1
rm -rf hfsutils
cp -r $SRC/hfsutils .
cd hfsutils
./configure --prefix=$PREFIX
make 
make install
cd ..

runhaskell ../Retro68/PrepareHeaders.hs ../Retro68/Universal\ Headers toolchain/m68k-unknown-elf/include

mkdir -p build-host
cd build-host
cmake ../../Retro68/ -DCMAKE_INSTALL_PREFIX=$PREFIX
cd ..

mkdir -p build-target
cd build-target
cmake ../../Retro68/ -DCMAKE_INSTALL_PREFIX=$PREFIX/m68k-unknown-elf \
					-DCMAKE_TOOLCHAIN_FILE=$SRC/retro68.toolchain.cmake \
					-DRETRO68_ROOT=$PREFIX \
					-DCMAKE_BUILD_TYPE=Release
cd ..

make -C build-host install
make -C build-target install