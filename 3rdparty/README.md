## How to recreate the qmake base buildsystem for the 3rd-party stuff

## openssl

### Linux

* `./config no-asm no-idea`
* `make`
* `cp crypto/buildinf.h libcrypto-include-unix`
* `cp include/openssl/opensslconf.h libcrypto-include-unix/openssl`

### Mac OS X

* `./Configure darwin64-x86_64-cc no-asm no-idea`
* `make depend`
* `make`
* `cp crypto/buildinf.h libcrypto-include-osx`
* `cp include/openssl/opensslconf.h libcrypto-include-osx/openssl`

### Windows
* `perl Configure VC-WIN32 no-asm no-idea`
* `ms\do_ms.bat`
* `nmake -f ms\nt.mak`
* `copy /Y crypto\buildinf.h libarchive-include-win32`
* `copy /Y inc32\openssl\*.h libarchive-include-win32\openssl`


## xz

### Linux
* ` ./configure --disable-shared --enable-threads=no --disable-assembler --disable-xz --disable-xzdec
                --disable-lzmadec --disable-lzmainfo --disable-lzma-links --disable-scripts --disable-doc`
* `cp config.h config-unix.h`
* `echo "#include PLATFORM_CONFIG_H" >config.h`

### Mac OS X
* ` ./configure --disable-shared --enable-threads=no --disable-assembler --disable-xz --disable-xzdec
                --disable-lzmadec --disable-lzmainfo --disable-lzma-links --disable-scripts --disable-doc`
* `cp config.h config-osx.h`
* `echo "#include PLATFORM_CONFIG_H" >config.h`

### Windows
* `copy /Y windows\config.h config-windows.h`
* `echo #include PLATFORM_CONFIG_H >config.h`


## libarchive

### Linux
* `./configure --disable-shared --disable-bsdtar --disable-bsdcpio --disable-xattr --disable-acl
               --without-bz2lib --without-lzmadec --without-lzo2 --without-nettle --without-openssl
               --without-xml2 --without-expat`
* `cp config.h config-unix.h`

### Mac OS X
* `./configure --disable-shared --disable-bsdtar --disable-bsdcpio --disable-xattr --disable-acl
               --without-bz2lib --without-lzmadec --without-lzo2 --without-nettle --without-openssl
               --without-xml2 --without-expat`
* `cp config.h config-osx.h`

### Windows
Prerequisite: build zlib 1.2.8

* `mkdir cbuild`
* `cd cbuild`
* `cmake -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_XATTR=OFF -DENABLE_ACL=OFF -DENABLE_TEST=OFF
         -DENABLE_OPENSSL=OFF -DZLIB_LIBRARY=..\..\zlib-1.2.8\release\z.lib -DZLIB_INCLUDE_DIR=..\..\zlib-1.2.8`
* `copy /Y config.h config-windows.h`
