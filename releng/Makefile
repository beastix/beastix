include releng-config.mk

clean-build:
	rm -rf ${RELEASE_BUILD}/*

clean-final:
	rm -rf ${RELEASE_FINAL}

release-final-dir:
	mkdir ${RELEASE_FINAL}

minimal-root:
	mkdir -p ${RELEASE_BUILD}/minroot
	cp -R --preserve ${SRC_ROOT}/world/rootfs/*         ${RELEASE_BUILD}/minroot/
	cp -R --preserve ${SRC_ROOT}/obj/musl/_install/*    ${RELEASE_BUILD}/minroot/
	cp -R --preserve ${SRC_ROOT}/obj/busybox/_install/* ${RELEASE_BUILD}/minroot/
	rm ${RELEASE_BUILD}/minroot/linuxrc
	cd ${RELEASE_BUILD}/minroot; ln -sf /lib/libc.so bin/ldd
	cd ${RELEASE_BUILD}/minroot; ln -sf /lib/libc.so lib/ld-musl-x86_64.so.1
	mkdir -p ${RELEASE_BUILD}/minroot/usr
	rm -rf ${RELEASE_BUILD}/minroot/usr/include
	cd ${RELEASE_BUILD}/minroot/usr; ln -sf ../include

base-root: minimal-root
	mkdir -p ${RELEASE_BUILD}/baseroot
	cp -R --preserve ${RELEASE_BUILD}/minroot/*             ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${RELEASE_BUILD}/bsdtools/_install/*   ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/gcc/_install/*         ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/binutils/_install/*    ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/make/_install/*        ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/util-linux/_install/*  ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/syslinux/_install/*    ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/headers/*              ${RELEASE_BUILD}/baseroot/usr/include/
	cp -R --preserve ${SRC_ROOT}/obj/m4/_install/*          ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/flex/_install/*        ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/bison/_install/*       ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/obj/bc/_install/*          ${RELEASE_BUILD}/baseroot/


base-tarball: base-root
	cd ${RELEASE_BUILD}/baseroot; tar cvf ../base.tar ./*
	gzip ${RELEASE_BUILD}/base.tar

live-initrd: base-root
	cd ${RELEASE_BUILD}/baseroot; find . | ${SRC_ROOT}/bootstrap/tools/bin/cpio -H newc -o | gzip > ${RELEASE_BUILD}/live_initrd.img

installer-initrd: minimal-root
	mkdir -p ${RELEASE_BUILD}/installerroot
	cp -R --preserve ${RELEASE_BUILD}/minroot/*          ${RELEASE_BUILD}/installerroot/
	cp -R --preserve ${SRC_ROOT}/installer/_install/*    ${RELEASE_BUILD}/installerroot/
	cp -R --preserve ${SRC_ROOT}/obj/syslinux/_install/* ${RELEASE_BUILD}/installerroot/
	cd ${RELEASE_BUILD}/installerroot; find . | ${SRC_ROOT}/bootstrap/tools/bin/cpio -H newc -o | gzip > ${RELEASE_BUILD}/installer_initrd.img

source-tarball:
	git clone --single-branch ${SRC_ROOT} ${RELEASE_BUILD}/src
	cd ${RELEASE_BUILD}/src; tar cvf ../src.tar ./*
	gzip ${RELEASE_BUILD}/src.tar

ports-tarball:
	git clone --single-branch ${PORTS_ROOT} ${RELEASE_BUILD}/ports
	cd ${RELEASE_BUILD}/ports; tar cvf ../ports.tar ./*
	gzip ${RELEASE_BUILD}/ports.tar

place-docs:
	mkdir -p ${RELEASE_BUILD}/doc
	cp ${SRC_ROOT}/README ${RELEASE_BUILD}/doc/

checksums:
	cd ${RELEASE_FINAL}; sh -c "md5sum * >MD5SUMS; exit 0"
	cd ${RELEASE_FINAL}; sh -c "sha256sum * >SHA256SUMS; exit 0"

release-final: release-final-dir base-tarball live-initrd installer-initrd source-tarball place-docs
	cp -v ${RELEASE_BUILD}/base.tar.gz                 ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/live_initrd.img             ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/installer_initrd.img        ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/src.tar.gz                  ${RELEASE_FINAL}/
	cp -rv ${RELEASE_BUILD}/doc                        ${RELEASE_FINAL}/doc
	cp ${SRC_ROOT}/obj/kernel/arch/x86_64/boot/bzImage ${RELEASE_FINAL}/

release-final-selfhost: release-final-dir base-tarball live-initrd installer-initrd place-docs
	cp -v ${RELEASE_BUILD}/base.tar.gz                 ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/live_initrd.img             ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/installer_initrd.img        ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/src.tar.gz                  ${RELEASE_FINAL}/
	cp -rv ${RELEASE_BUILD}/doc                        ${RELEASE_FINAL}/doc
	cp ${SRC_ROOT}/obj/kernel/arch/x86_64/boot/bzImage ${RELEASE_FINAL}/


iso-root: checksums
	mkdir -p ${RELEASE_BUILD}/isoroot
	cp -R --preserve ${SRC_ROOT}/releng/cd_templ/* ${RELEASE_BUILD}/isoroot
	cp -R --preserve ${RELEASE_FINAL}/* ${RELEASE_BUILD}/isoroot/

iso-image: iso-root 
	make -C ${SRC_ROOT}/obj/kernel isoimage
	cp ${SRC_ROOT}/obj/kernel/arch/x86/boot/isoimage/isolinux.bin ${RELEASE_BUILD}/isoroot/
	genisoimage -l -J -r -o ${RELEASE_FINAL}/beastix.iso -b isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table ${RELEASE_BUILD}/isoroot
	isohybrid ${RELEASE_FINAL}/beastix.iso

blank-hdimage:
	dd if=/dev/zero of=${RELEASE_BUILD}/beastix_hdd.img bs=1M count=8192

clean: clean-build clean-final
