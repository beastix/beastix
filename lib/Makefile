build-musl:
	cd musl; ./configure --prefix=../../obj/lib/musl/_install --disable-gcc-wrapper
	make -C musl
	make -C musl install
	cd ../../obj/lib/musl/_install; ln -s /lib/libc.so bin/ldd; ln -sf /lib/libc.so lib/ld-musl-x86_64.so.1

bootstrap-musl:
	export PATH=`pwd`/../bootstrap/tools/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; cd musl; ./configure --host="x86_64-unknown-linux-musl" --prefix=`pwd`/../../bootstrap/musl/_install --syslibdir=`pwd`/../../bootstrap/musl/_install/lib --target="x86_64-unknown-linux-musl" --disable-gcc-wrapper --disable-shared --enable-static
	export PATH=`pwd`/../bootstrap/gcc/_install/bin:`pwd`/../bootstrap/binutils/_install/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; make -C musl install

build-bootstrap: bootstrap-musl

build-libedit: build-musl
	cd ../obj/lib/libedit; export PATH=`pwd`/../bootstrap/tools/bin/:`pwd`/../bootstrap/tools/x86_64-unknown-linux-musl/bin:/bin:/usr/bin;  export LDFLAGS="-static"; export CFLAGS="-static"; export CPPFLAGS=-I`pwd`/../bootstrap/tools/include/; ../../../lib/libedit/configure --host=x86_64-unknown-linux-musl --target=x86_64-unknown-linux-musl

build: build-musl build-libedit
