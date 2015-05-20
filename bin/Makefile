build-busybox:
	make -C busybox/ O=../../obj/bin/busybox/  defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ../obj/bin/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ../obj/bin/busybox/.config
	make -C ../obj/bin/busybox/ 
	make -C ../obj/bin/busybox  PREFIX=. install

build-binutils:
	cd ../obj/bin/binutils; ../../../bin/binutils/configure --prefix=`pwd` --with-sysroot --disable-nls --disable-werror
	make -C ../obj/bin/binutils
	make -C ../obj/bin/binutils PREFIX=. install

build-gcc:
	export PATH=`pwd`/../obj/bin/binutils/bin:`pwd`/../obj/bin/busybox/_install/bin:`pwd`/../obj/bin/busybox/_install/usr/bin:/usr/bin;  cd ../obj/bin/gcc; ../../../bin/gcc/configure --prefix=`pwd` --disable-nls --enable-languages=c,c++ --without-headers
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; export PATH=`pwd`/../obj/bin/binutils/bin:`pwd`/../obj/bin/busybox/_install/bin:`pwd`/../obj/bin/busybox/_install/usr/bin:/usr/bin;  make -C ../obj/bin/gcc all-gcc
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; export PATH=`pwd`/../obj/bin/binutils/bin:`pwd`/../obj/bin/busybox/_install/bin:`pwd`/../obj/bin/busybox/_install/usr/bin:/usr/bin;  make -C ../obj/bin/gcc all-target-libgcc
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; export PATH=`pwd`/../obj/bin/binutils/bin:`pwd`/../obj/bin/busybox/_install/bin:`pwd`/../obj/bin/busybox/_install/usr/bin:/usr/bin;  make -C ../obj/bin/gcc install-gcc
	export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/; export PATH=`pwd`/../obj/bin/binutils/bin:`pwd`/../obj/bin/busybox/_install/bin:`pwd`/../obj/bin/busybox/_install/usr/bin:/usr/bin;  make -C ../obj/bin/gcc install-target-libgcc

build: build-busybox build-binutils build-gcc
