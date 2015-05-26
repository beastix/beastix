WORLDENV := export PATH="${BOOTSTRAP_PATH}"; export CFLAGS="${WORLD_CFLAGS}"; export LDFLAGS="${WORLD_LDFLAGS}"; export CC="${WORLD_CC}";

buildworld-busybox:
	mkdir -p ${WORLD_BUILD}/busybox
	make -C ${SRC_ROOT}/world/busybox O=${WORLD_BUILD}/busybox defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" -i ${WORLD_BUILD}/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i ${WORLD_BUILD}/busybox/.config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i ${WORLD_BUILD}/busybox/.config
	make -C ${WORLD_BUILD}/busybox/ 
	make -C ${WORLD_BUILD}/busybox/ PREFIX=. install

buildworld-gcc: 
	mkdir -p ${WORLD_BUILD}/gcc/_install
	${WORLDENV} cd ${WORLD_BUILD}/gcc; ${SRC_ROOT}/world/gcc/configure ${WORLD_CONFIG}  --enable-languages=c --disable-nls --with-newlib --disable-multilib --disable-libssp \
                                                                                            --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared --disable-libmudflap \
                                                                                            --disable-libgomp --prefix=${WORLD_BUILD}/gcc/_install
	${WORLDENV} make -C ${WORLD_BUILD}/gcc all-gcc install-gcc CC=${WORLD_CC} CC_FOR_BUILD=${WORLD_CC}
	${WORLDENV} make -C ${WORLD_BUILD}/gcc all-target-libgcc install-gcc install-target-libgcc

buildworld-musl:
	mkdir -p ${WORLD_BUILD}/musl/_install
	cd ${SRC_ROOT}/world/musl; ./configure --prefix=${WORLD_BUILD}/musl/_install --disable-gcc-wrapper
	make -C ${SRC_ROOT}/world/musl
	make -C ${SRC_ROOT}/world/musl install
	mkdir -p ${WORLD_BUILD}/musl/_install/bin
	cd ${WORLD_BUILD}/musl/_install; ln -sf lib/libc.so bin/ldd; ln -sf lib/libc.so lib/ld-musl-x86_64.so.1

buildworld-make:
	mkdir -p ${WORLD_BUILD}/make/_install
	${WORLDENV} cd ${WORLD_BUILD}/make; ${SRC_ROOT}/world/make/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/make/_install --disable-nls --without-guile --without-libiconv
	${WORLDENV} make -C ${WORLD_BUILD}/make
	${WORLDENV} make -C ${WORLD_BUILD}/make install

buildworld-util-linux:
	mkdir -p ${WORLD_BUILD}/util-linux/_install
	${WORLDENV} cd ${WORLD_BUILD}/util-linux; ${SRC_ROOT}/world/util-linux/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/util-linux/_install --disable-nls --enable-static --disable-rpath \
	                                                                                                 --disable-all-programs --disable-bash-completion --disable-makeinstall-setuid --without-selinux \
	                                                                                                 --without-udev --without-libiconv --without-libintl-prefix --without-slang \
	                                                                                                 --without-ncurses --without-utempter --without-user --without-systemd --without-smack \
	                                                                                                 --without-python --enable-libuuid
	${WORLDENV} make -C ${WORLD_BUILD}/util-linux
	${WORLDENV} make -C ${WORLD_BUILD}/util-linux install

buildworld-syslinux:
	mkdir -p ${WORLD_BUILD}/syslinux/_install
	${WORLDENV} make -C ${SRC_ROOT}/world/syslinux CC=${WORLD_CC} O=${WORLD_BUILD}/syslinux 
	${WORLDENV} make -C ${SRC_ROOT}/world/syslinux CC=${WORLD_CC} install O=${WORLD_BUILD}/syslinux INSTALLROOT=${WORLD_BUILD}/syslinux/_install

buildworld-binutils:
	mkdir -p ${WORLD_BUILD}/binutils/_install
	${WORLDENV} cd ${WORLD_BUILD}/binutils; ${SRC_ROOT}/world/binutils/configure ${WORLD_CONFIG} --prefix=${WORLD_BUILD}/binutils/_install --disable-install-libbfd --disable-shared
	${WORLDENV} make -C ${WORLD_BUILD}/binutils
	${WORLDENV} make -C ${WORLD_BUILD}/binutils install-gas install-ld install-binutils

clean-world:
	make -i -C ${WORLD_BUILD}/binutils distclean clean
	make -i -C ${WORLD_BUILD}/gcc distclean clean
	make -i -C ${WORLD_BUILD}/musl distclean clean	

buildworld: bootstrap buildworld-musl buildworld-busybox buildworld-binutils buildworld-gcc buildworld-make buildworld-util-linux

installworld:
