bootstrap-binutils:
	cd ${SRC_ROOT}/bootstrap/binutils; ${SRC_ROOT}/world/binutils/configure ${BOOTSTRAP_CONFIG} --disable-werror --without-docdir
	echo "MAKEINFO = :" >> ${SRC_ROOT}/bootstrap/binutils/Makefile
	${MAKE} -C ${SRC_ROOT}/bootstrap/binutils all
	${MAKE} -C ${SRC_ROOT}/bootstrap/binutils install-gas install-ld install-binutils

bootstrap-gcc:
	cd ${SRC_ROOT}/bootstrap/gcc; ${SRC_ROOT}/world/gcc/configure ${BOOTSTRAP_CONFIG} --build=x86_64-unknown-linux-musl --enable-languages=c --with-newlib --disable-multilib \
                                                                                          --disable-libssp --disable-libquadmath --disable-threads --disable-decimal-float --disable-shared \
                                                                                          --disable-libmudflap --disable-libgomp --disable-werror --without-docdir
	echo "MAKEINFO = :" >> ${SRC_ROOT}/bootstrap/gcc/Makefile
	make -C ${SRC_ROOT}/bootstrap/gcc all-gcc install-gcc
	make -C ${SRC_ROOT}/bootstrap/gcc all-target-libgcc install-target-libgcc
	cd ${BOOTSTRAP_TOOLS}/bin; ln -sf x86_64-unknown-linux-musl-gcc x86_64-unknown-linux-musl-cc

bootstrap-musl:
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; export CFLAGS=-fPIC; cd ${SRC_ROOT}/world/musl; ./configure ${BOOTSTRAP_CONFIG} --syslibdir=${BOOTSTRAP_TOOLS}/lib \
	                                                                                                                    --host="x86_64-unknown-linux-musl" \
	                                                                                                                    --disable-gcc-wrapper --disable-shared --enable-static
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/world/musl install

bootstrap-util-linux:
	export CFLAGS=-fPIC; cd ${SRC_ROOT}/bootstrap/util-linux; ${SRC_ROOT}/world/util-linux/configure ${BOOTSTRAP_CONFIG}  --disable-nls --enable-static \
	                                                                                                 --disable-rpath \
	                                                                                                 --disable-all-programs --disable-bash-completion --disable-makeinstall-setuid --without-selinux \
	                                                                                                 --without-udev --without-libiconv --without-libintl-prefix --without-slang \
	                                                                                                 --without-ncurses --without-utempter --without-user --without-systemd --without-smack \
	                                                                                                 --without-python --enable-libuuid
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/util-linux
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/util-linux install

bootstrap-linux-headers:
	make -C ${SRC_ROOT}/obj/kernel INSTALL_HDR_PATH=${BOOTSTRAP_TOOLS} headers_install
	cd ${BOOTSTRAP_TOOLS}; mkdir -p usr; ln -sf ${BOOTSTRAP_TOOLS}/include usr/include

bootstrap-nasm:
	cd ${SRC_ROOT}/bootstrap/nasm; ${SRC_ROOT}/world/nasm/configure ${BOOTSTRAP_CONFIG}
	mkdir -p ${SRC_ROOT}/bootstrap/nasm/lib;
	touch ${SRC_ROOT}/world/nasm/*.c
	touch ${SRC_ROOT}/world/nasm/*.h
	cp ${SRC_ROOT}/world/nasm/*.c ${SRC_ROOT}/bootstrap/nasm/
	cp ${SRC_ROOT}/world/nasm/*.h ${SRC_ROOT}/bootstrap/nasm/
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/nasm nasm 
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/nasm install

bootstrap-cpio:
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; cd ${SRC_ROOT}/bootstrap/cpio; ${SRC_ROOT}/world/cpio/configure ${BOOTSTRAP_CONFIG} --disable-nls --without-libintl-prefix
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/cpio
	export PATH=${BOOTSTRAP_PATH}; export CC=${BOOTSTRAP_CC}; ${MAKE} -C ${SRC_ROOT}/bootstrap/cpio install

	

bootstrap-syslinux:
	@echo syslinux not yet integrated

clean-bootstrap:
	make -i -C ${SRC_ROOT}/bootstrap/binutils distclean clean
	make -i -C ${SRC_ROOT}/bootstrap/gcc distclean clean
	make -i -C ${SRC_ROOT}/world/musl distclean clean
	rm -rf ${SRC_ROOT}/tools/*

bootstrap: bootstrap-binutils bootstrap-gcc bootstrap-linux-headers bootstrap-musl bootstrap-util-linux bootstrap-nasm bootstrap-syslinux bootstrap-cpio
