build-busybox:
	make -C busybox/ O=../../obj/bin/busybox/  defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ../obj/bin/busybox/.config
	make -C ../obj/bin/busybox/ 
	make -C ../obj/bin/busybox  PREFIX=. install

build-binutils:
	export PATH=`pwd`/../bootstrap/tools/bin:/bin:/usr/bin; cd ../obj/bin/binutils; ../../../bin/binutils/configure --target=x86_64-unknown-linux-musl --prefix=`pwd`/_install --disable-nls --disable-shared --disable-werror --disable-install-libbfd --host=x86_64-unknown-linux-musl
	rm -rf ../obj/bin/binutils/_install
	mkdir ../obj/bin/binutils/_install
	export PATH=`pwd`/../bootstrap/binutils/_install/x86_64-unknown-linux-musl/bin:`pwd`/../bootstrap/gcc/_install/bin:/bin:/usr/bin; export CFLAGS="-ffunction-sections -fdata-sections"; export LDFLAGS="-s"; export CC="x86_64-unknown-linux-musl-gcc"; make -C ../obj/bin/binutils install-gas install-ld install-binutils tooldir=""
	export PATH=`pwd`/../bootstrap/binutils/_install/bin:`pwd`/../bootstrap/gcc/_install/bin:/bin:/usr/bin; export CFLAGS="-Os"; export LDFLAGS="-s"; export CC="x86_64-unknown-linux-musl-gcc"; make -C ../obj/bin/binutils install


build-gcc:
	cd ../obj/bin/gcc; ../../../bin/gcc/configure --prefix=`pwd` --disable-nls --enable-languages=c,c++ --without-headers --enable-static
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; make -C ../obj/bin/gcc all
	make -C ../obj/bin/gcc install

bootstrap-binutils:
	cd ../bootstrap; mkdir binutils; cd binutils; ../../bin/binutils/configure --target=x86_64-unknown-linux-musl --prefix=`pwd`/_install --disable-install-libbfd --with-sysroot=`pwd`/../tools
	rm -rf ../bootstrap/binutils/_install
	mkdir ../bootstrap/binutils/_install
	make -C ../bootstrap/binutils
	make -C ../bootstrap/binutils install

bootstrap-gcc: 
	cd ../bootstrap; mkdir gcc; cd gcc; ../../bin/gcc/configure --target=x86_64-unknown-linux-musl  --enable-languages=c --with-newlib --disable-multilib --disable-libssp --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared --disable-libmudflap --disable-libgomp --with-sysroot=`pwd`/../tools --prefix=`pwd`/_install
	export PATH=`pwd`/../bootstrap/binutils/_install/bin/:`pwd`/../bootstrap/binutils/_install/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-gcc install-gcc
	export PATH=`pwd`/../bootstrap/binutils/_install/bin:`pwd`/../bootstrap/binutils/_install/x86_64-unknown-linux-musl/bin:/bin:/usr/bin; make -C ../bootstrap/gcc all-target-libgcc install-gcc install-target-libgcc

build-bootstrap: bootstrap-binutils bootstrap-gcc

build: build-busybox build-binutils build-gcc
