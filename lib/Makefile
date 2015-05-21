build-musl:
	cd musl; ./configure --prefix=../../obj/lib/musl
	make -C ../../obj/lib/musl

bootstrap-musl:
	cd ../bootstrap; mkdir musl; cd musl; ../../lib/musl/configure --host="x86_64-unknown-linux-musl" --prefix=`pwd`/_install --syslibdir=`pwd`/_install --disable-gcc-wrapper --disable-shared
	make install

build-bootstrap: bootstrap-musl

build: build-musl
