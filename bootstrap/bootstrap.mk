bootstrap-binutils:
	cd ${SRC_ROOT}/bootstrap/binutils; ${SRC_ROOT}/world/binutils/configure ${BOOTSTRAP_CONFIG}
	make -C ${SRC_ROOT}/bootstrap/binutils all
	make -C ${SRC_ROOT}/bootstrap/binutils install-gas install-ld install-binutils

bootstrap-gcc:
	cd ${SRC_ROOT}/bootstrap/gcc; ${SRC_ROOT}/world/gcc/configure ${BOOTSTRAP_CONFIG} --build=x86_64-unknown-linux-musl --enable-languages=c --with-newlib --disable-multilib \
                                                                                          --disable-libssp --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared \
                                                                                          --disable-libmudflap --disable-libgomp
	make -C ${SRC_ROOT}/bootstrap/gcc all-gcc install-gcc
	make -C ${SRC_ROOT}/bootstrap/gcc all-target-libgcc install-gcc install-target-libgcc
	cd ${BOOTSTRAP_TOOLS}/bin; ln -sf x86_64-unknown-linux-musl-gcc x86_64-unknown-linux-musl-cc

bootstrap-musl:
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; export CFLAGS=-fPIC; cd ${SRC_ROOT}/world/musl; ./configure ${BOOTSTRAP_CONFIG} --syslibdir=${BOOTSTRAP_TOOLS}/lib \
	                                                                                                                    --host="x86_64-unknown-linux-musl" \
	                                                                                                                    --disable-gcc-wrapper --disable-shared --enable-static
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; make -C ${SRC_ROOT}/world/musl install

bootstrap-util-linux:
	export CFLAGS=-fPIC; cd ${SRC_ROOT}/bootstrap/util-linux; ${SRC_ROOT}/world/util-linux/configure ${BOOTSTRAP_CONFIG} --prefix=${WORLD_BUILD}/util-linux/_install --disable-nls --enable-static \
	                                                                                                 --disable-rpath \
	                                                                                                 --disable-all-programs --disable-bash-completion --disable-makeinstall-setuid --without-selinux \
	                                                                                                 --without-udev --without-libiconv --without-libintl-prefix --without-slang \
	                                                                                                 --without-ncurses --without-utempter --without-user --without-systemd --without-smack \
	                                                                                                 --without-python --enable-libuuid
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; make -C ${SRC_ROOT}/bootstrap/util-linux
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; make -C ${SRC_ROOT}/bootstrap/util-linux install

bootstrap-syslinux:
	@echo syslinux not yet integrated

clean-bootstrap:
	make -i -C ${SRC_ROOT}/bootstrap/binutils distclean clean
	make -i -C ${SRC_ROOT}/bootstrap/gcc distclean clean
	make -i -C ${SRC_ROOT}/world/musl distclean clean
	rm -rf ${SRC_ROOT}/tools/*

bootstrap: bootstrap-musl bootstrap-binutils bootstrap-gcc bootstrap-syslinux
	cd ${BOOTSTRAP_TOOLS}; mkdir -p usr; ln -sf include usr/include
