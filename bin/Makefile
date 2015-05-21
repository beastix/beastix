build-busybox:
	make -C busybox/ O=../../obj/bin/busybox/  defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ../obj/bin/busybox/.config
	make -C ../obj/bin/busybox/ 
	make -C ../obj/bin/busybox  PREFIX=. install

build-binutils:
	cd ../obj/bin/binutils; ../../../bin/binutils/configure --prefix=`pwd` --disable-nls --disable-werror --disable-shared
	make -C ../obj/bin/binutils clean
	export LDFLAGS="-all-static"; make -C ../obj/bin/binutils configure-host
	export LDFLAGS="-all-static"; make -C ../obj/bin/binutils all LDFLAGS="-all-static"
	export LDFLAGS="-all-static"; make -C ../obj/bin/binutils PREFIX=. install

build-gcc:
	cd ../obj/bin/gcc; ../../../bin/gcc/configure --prefix=`pwd` --disable-nls --enable-languages=c,c++ --without-headers --enable-static
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; make -C ../obj/bin/gcc all
	make -C ../obj/bin/gcc install

bootstrap-binutils:
	cd ../bootstrap; mkdir binutils; cd binutils; ../../binutils/configure --target=x86_64-unknown-linux-musl --prefix=`pwd`/_install --disable-install-libbfd
	make -C ../bootstrap/binutils install-gas install-ld install-binutils

bootstrap-gcc:

build-bootstrap: bootstrap-binutils bootstrap-gcc

build: build-busybox build-binutils build-gcc
