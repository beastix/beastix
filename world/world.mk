WORLDENV := export PATH="${BOOTSTRAP_PATH}"; export CFLAGS="${WORLD_CFLAGS}"; export LDFLAGS="${WORLD_LDFLAGS}"; export CC="${WORLD_CC}"; export CPPFLAGS="${WORLD_CPPFLAGS}";

buildworld-busybox:
	mkdir -p ${WORLD_BUILD}/busybox
	make -C ${SRC_ROOT}/world/busybox O=${WORLD_BUILD}/busybox defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" -i ${WORLD_BUILD}/busybox/.config
	sed -i "s/.*CONFIG_SED.*/CONFIG_SED=n/" -i ${WORLD_BUILD}/busybox/.config
	sed -i 's/.*CONFIG_FEATURE_IPV6.*/CONFIG_FEATURE_IPV6=n/' -i ${WORLD_BUILD}/busybox/.config
	sed -i 's/.*CONFIG_BRCTL.*/CONFIG_BRCTL=n/' -i ${WORLD_BUILD}/busybox/.config
	sed -i 's/.*CONFIG_IFPLUGD.*/CONFIG_IFPLUGD=n/' -i ${WORLD_BUILD}/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ${WORLD_BUILD}/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ${WORLD_BUILD}/busybox/.config
	${MAKE} -C ${WORLD_BUILD}/busybox/ 
	${MAKE} -C ${WORLD_BUILD}/busybox/ PREFIX=. install

# this will eventually be replaced with the whole bsdtools suite
buildworld-sed:
	mkdir -p ${WORLD_BUILD}/bsdtools/_install/usr/bin
	${WORLDENV} ${WORLD_CC} ${WORLD_CFLAGS} -nostdinc -I${SRC_ROOT}/world/bsdtools/include ${SRC_ROOT}/world/bsdtools/sed/*.c -o ${WORLD_BUILD}/bsdtools/_install/usr/bin/sed

# gcc can't build multicore
buildworld-gcc: 
	mkdir -p ${WORLD_BUILD}/gcc/_install
	${WORLDENV} cd ${WORLD_BUILD}/gcc; ${SRC_ROOT}/world/gcc/configure ${WORLD_CONFIG}  --enable-languages=c --disable-nls --with-newlib --disable-multilib --disable-libssp \
                                                                                            --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared --disable-libmudflap \
                                                                                            --disable-libgomp --prefix=${WORLD_BUILD}/gcc/_install --disable-werror --without-docdir
	echo "MAKEINFO = :" >> ${SRC_ROOT}/obj/gcc/Makefile
	${WORLDENV} make -C ${WORLD_BUILD}/gcc all-gcc install-gcc CC=${WORLD_CC} CC_FOR_BUILD=${WORLD_CC}
	${WORLDENV} make -C ${WORLD_BUILD}/gcc all-target-libgcc install-gcc install-target-libgcc

buildworld-musl:
	mkdir -p ${WORLD_BUILD}/musl/_install
	cd ${SRC_ROOT}/world/musl; ./configure --prefix=${WORLD_BUILD}/musl/_install --disable-gcc-wrapper
	${MAKE} -C ${SRC_ROOT}/world/musl
	${MAKE} -C ${SRC_ROOT}/world/musl install
	mkdir -p ${WORLD_BUILD}/musl/_install/bin
	cd ${WORLD_BUILD}/musl/_install; ln -sf lib/libc.so bin/ldd; ln -sf lib/libc.so lib/ld-musl-x86_64.so.1

buildworld-make:
	mkdir -p ${WORLD_BUILD}/make/_install
	${WORLDENV} cd ${WORLD_BUILD}/make; ${SRC_ROOT}/world/make/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/make/_install --disable-nls --without-guile --without-libiconv
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/make
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/make install

buildworld-util-linux:
	mkdir -p ${WORLD_BUILD}/util-linux/_install
	${WORLDENV} cd ${WORLD_BUILD}/util-linux; ${SRC_ROOT}/world/util-linux/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/util-linux/_install --disable-nls --enable-static --disable-rpath \
	                                                                                                 --disable-all-programs --disable-bash-completion --disable-makeinstall-setuid --without-selinux \
	                                                                                                 --without-udev --without-libiconv --without-libintl-prefix --without-slang \
	                                                                                                 --without-ncurses --without-utempter --without-user --without-systemd --without-smack \
	                                                                                                 --without-python --enable-libuuid
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/util-linux all
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/util-linux install

buildworld-syslinux:
	mkdir -p ${WORLD_BUILD}/syslinux/_install
	mkdir -p ${WORLD_BUILD}/syslinux/bios/core/
	cp ${SRC_ROOT}/world/syslinux/bios/core/ldlinux.bin ${WORLD_BUILD}/syslinux/bios/core/ldlinux.bin
	cp ${SRC_ROOT}/world/syslinux/bios/core/isolinux.bin ${WORLD_BUILD}/syslinux/bios/core/isolinux.bin
	cp ${SRC_ROOT}/world/syslinux/bios/core/isolinux-debug.bin ${WORLD_BUILD}/syslinux/bios/core/isolinux-debug.bin
	cp ${SRC_ROOT}/world/syslinux/bios/version.gen ${WORLD_BUILD}/syslinux/bios/version.gen
	cp ${SRC_ROOT}/world/syslinux/kwdhash.gen ${WORLD_BUILD}/syslinux/bios/core/kwdhash.gen
	cp -Rv ${SRC_ROOT}/world/syslinux/codepage ${WORLD_BUILD}/syslinux/bios
	${WORLDENV} make -C ${SRC_ROOT}/world/syslinux CC=${WORLD_CC} O=${WORLD_BUILD}/syslinux 
	${WORLDENV} make -C ${SRC_ROOT}/world/syslinux CC=${WORLD_CC} install O=${WORLD_BUILD}/syslinux INSTALLROOT=${WORLD_BUILD}/syslinux/_install

buildworld-binutils:
	mkdir -p ${WORLD_BUILD}/binutils/_install
	${WORLDENV} cd ${WORLD_BUILD}/binutils; ${SRC_ROOT}/world/binutils/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/binutils/_install --disable-install-libbfd --disable-shared \
	                                                                                             --disable-werror --without-docdir
	echo "MAKEINFO = :" >> ${SRC_ROOT}/obj/binutils/Makefile
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/binutils
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/binutils install-gas install-ld install-binutils

clean-world:
	make -i -C ${WORLD_BUILD}/binutils distclean clean
	make -i -C ${WORLD_BUILD}/gcc distclean clean
	make -i -C ${WORLD_BUILD}/musl distclean clean	

fixheaders:
	mkdir -p ${WORLD_BUILD}/headers
	cp -Rv ${SRC_ROOT}/bootstrap/tools/include/* ${WORLD_BUILD}/headers/

buildworld-m4:
	mkdir -p ${WORLD_BUILD}/m4/_install
	${WORLDENV} cd ${WORLD_BUILD}/m4; ${SRC_ROOT}/world/m4/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/m4/_install
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/m4
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/m4 install

buildworld-flex:
	touch ${SRC_ROOT}/world/flex/doc/*
	mkdir -p ${WORLD_BUILD}/flex/_install
	${WORLDENV} cd ${WORLD_BUILD}/flex; ${SRC_ROOT}/world/flex/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/flex/_install
	echo "MAKEINFO = :" >> ${SRC_ROOT}/obj/flex/Makefile
	touch ${WORLD_BUILD}/flex/doc/flex.pdf
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/flex
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/flex install

buildworld-bison:
	mkdir -p ${WORLD_BUILD}/bison/examples
	touch ${SRC_ROOT}/world/bison/doc/bison.help
	touch ${SRC_ROOT}/world/bison/doc/bison.1
	touch ${WORLD_BUILD}/bison/examples/extracted.stamp
	mkdir -p ${WORLD_BUILD}/bison/_install
	${WORLDENV} cd ${WORLD_BUILD}/bison; ${SRC_ROOT}/world/bison/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/bison/_install
	${WORLDENV} ${MAKE} -i -C ${WORLD_BUILD}/bison
	${WORLDENV} ${MAKE} -i -C ${WORLD_BUILD}/bison install

buildworld-bc:
	mkdir -p ${WORLD_BUILD}/bc/_install
	${WORLDENV} cd ${WORLD_BUILD}/bc; ${SRC_ROOT}/world/bc/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/bc/_install
	echo "MAKEINFO = :" >> ${SRC_ROOT}/obj/bc/Makefile
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/bc
	${WORLDENV} ${MAKE} -C ${WORLD_BUILD}/bc install


buildworld:  fixheaders buildworld-m4 buildworld-flex buildworld-bison buildworld-bc buildworld-musl buildworld-busybox buildworld-binutils buildworld-gcc buildworld-make buildworld-util-linux buildworld-syslinux

installworld:
	cp -Ri -p ${SRC_ROOT}/world/rootfs/*             /
	cp -R  -p ${SRC_ROOT}/obj/musl/_install/*        /
	cp -R  -p ${SRC_ROOT}/obj/busybox/_install/*     /
	cp -R  -p ${SRC_ROOT}/obj/bsdtools/_install/*    /
	cp -R  -p ${SRC_ROOT}/obj/gcc/_install/*         /
	cp -R  -p ${SRC_ROOT}/obj/binutils/_install/*    /
	cp -R  -p ${SRC_ROOT}/obj/make/_install/*        /
	cp -R  -p ${SRC_ROOT}/obj/util-linux/_install/*  /
	cp -R  -p ${SRC_ROOT}/obj/syslinux/_install/*    /
	cp -R  -p ${SRC_ROOT}/obj/headers/*              /usr/include/
	cp -R  -p ${SRC_ROOT}/obj/m4/_install/*          /
	cp -R  -p ${SRC_ROOT}/obj/flex/_install/*        /
	cp -R  -p ${SRC_ROOT}/obj/bison/_install/*       /
	cp -R  -p ${SRC_ROOT}/obj/bc/_install/*          /

