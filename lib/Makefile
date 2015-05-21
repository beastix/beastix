build-musl:
	cd musl; ./configure --prefix=../../obj/lib/musl
	make -C ../../obj/lib/musl

bootstrap-musl:
	export PATH=`pwd`/../bootstrap/tools/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; cd musl; ./configure --host="x86_64-unknown-linux-musl" --prefix=`pwd`/../../bootstrap/musl/_install --syslibdir=`pwd`/../../bootstrap/musl/_install/lib --target="x86_64-unknown-linux-musl" --disable-gcc-wrapper --disable-shared --enable-static
	export PATH=`pwd`/../bootstrap/gcc/_install/bin:`pwd`/../bootstrap/binutils/_install/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; make -C musl install

build-bootstrap: bootstrap-musl

build: build-musl
