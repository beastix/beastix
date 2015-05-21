build-busybox:
	make -C busybox/ O=../../obj/bin/busybox/  defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ../obj/bin/busybox/.config
	make -C ../obj/bin/busybox/ 
	make -C ../obj/bin/busybox  PREFIX=. install

build-gcc:
	cd ../obj/bin/gcc; ../../../bin/gcc/configure --target=x86_64-unknown-linux-musl  --enable-languages=c --with-newlib --disable-multilib --disable-libssp --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared --disable-libmudflap --disable-libgomp --prefix=`pwd`/_install
	export PATH=`pwd`/../bootstrap/tools/bin/:`pwd`/../bootstrap/tools/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-gcc install-gcc
	export PATH=`pwd`/../bootstrap/tools/bin/:`pwd`/../bootstrap/tools/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-target-libgcc install-target-libgcc


bootstrap-binutils:
	cd ../bootstrap; mkdir binutils; cd binutils; ../../bin/binutils/configure --target=x86_64-unknown-linux-musl --prefix=`pwd`/_install --disable-install-libbfd --with-sysroot=`pwd`/../tools
	rm -rf ../bootstrap/binutils/_install
	mkdir ../bootstrap/binutils/_install
	make -C ../bootstrap/binutils
	make -C ../bootstrap/binutils install-gas install-ld install-binutils

build-binutils:
	export PATH=`pwd`/../bootstrap/tools/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; cd ../obj/bin/binutils; ../../../bin/binutils/configure --target=x86_64-unknown-linux-musl --prefix=`pwd`/_install --disable-install-libbfd --disable-shared
	rm -rf ../bootstrap/binutils/_install
	mkdir ../bootstrap/binutils/_install
	export PATH=`pwd`/../bootstrap/tools/bin:/bin:/usr/bin; export CC="x86_64-unknown-linux-musl-gcc"; export LDFLAGS="-static"; export CFLAGS="-static -I`pwd`/../bootstrap/tools/include/"; make -C ../obj/bin/binutils
	make -C ../obj/bin/binutils install-gas install-ld install-binutils


bootstrap-gcc: 
	cd ../bootstrap; mkdir gcc; cd gcc; ../../bin/gcc/configure --target=x86_64-unknown-linux-musl  --enable-languages=c --with-newlib --disable-multilib --disable-libssp --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared --disable-libmudflap --disable-libgomp --with-sysroot=`pwd`/../tools --prefix=`pwd`/../tools 
	export PATH=`pwd`/../bootstrap/binutils/_install/bin/:`pwd`/../bootstrap/binutils/_install/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-gcc install-gcc
	export PATH=`pwd`/../bootstrap/binutils/_install/bin:`pwd`/../bootstrap/binutils/_install/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-target-libgcc install-gcc install-target-libgcc

build-bootstrap: bootstrap-binutils bootstrap-gcc

build: build-busybox build-binutils build-gcc
