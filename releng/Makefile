include releng-config.mk

clean-build:
	rm -rf ${RELEASE_BUILD}/*

clean-final:
	rm -rf ${RELEASE_FINAL}

release-final-dir:
	mkdir ${RELEASE_FINAL}

minimal-root:
	mkdir ${RELEASE_BUILD}/minroot
	cp -R --preserve ${SRC_ROOT}/world/rootfs/*         ${RELEASE_BUILD}/minroot/
	cp -R --preserve ${SRC_ROOT}/obj/musl/_install/*    ${RELEASE_BUILD}/minroot/
	cp -R --preserve ${SRC_ROOT}/obj/busybox/_install/* ${RELEASE_BUILD}/minroot/
	rm ${RELEASE_BUILD}/minroot/linuxrc
	cd ${RELEASE_BUILD}/minroot; ln -sf /lib/libc.so bin/ldd
	cd ${RELEASE_BUILD}/minroot; ln -sf /lib/libc.so lib/ld-musl-x86_64.so.1
	mkdir -p ${RELEASE_BUILD}/minroot/usr
	cd ${RELEASE_BUILD}/minroot/usr; ln -sf ../include

base-root: minimal-root
	mkdir ${RELEASE_BUILD}/baseroot
	cp -R --preserve ${RELEASE_BUILD}/minroot/*            ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/world/gcc/_install/*      ${RELEASE_BUILD}/baseroot/
	cp -R --preserve ${SRC_ROOT}/world/binutils/_install/* ${RELEASE_BUILD}/baseroot/
#	cp -R --preserve ${SRC_ROOT}/world/make/_install/*     ${RELEASE_BUILD}/baseroot/
#	cp -R --preserve ${SRC_ROOT}/world/syslinux/_install/* ${RELEASE_BUILD}/baseroot/


base-tarball: base-root
	cd ${RELEASE_BUILD}/baseroot; tar cvf ../base.tar ./*
	gzip ${RELEASE_BUILD}/base.tar.gz

live-initrd: base-root
	cd ${RELEASE_BUILd}/baseroot; find . | cpio -H newc -o | gzip > ${RELEASE_BUILD}/live-initrd.img

installer-initrd: minimal-root
	@echo installer not yet implemented

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

checksums: release-final
	cd ${RELEASE_FINAL}; md5sum * >MD5SUMS
	cd ${RELEASE_FINAL}; sha256sum * >SHA256SUMS

release-final: release-final-dir base-tarball live-initrd installer-initrd source-tarball ports-tarball place-docs
	cp -v ${RELEASE_BUILD}/base.tar.gz                 ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/live-initrd.img             ${RELEASE_FINAL}/
#	cp -v ${RELEASE_BUILD}/installer_initrd.img        ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/source.tar.gz               ${RELEASE_FINAL}/
	cp -v ${RELEASE_BUILD}/ports.tar.gz                ${RELEASE_FINAL}/
	cp -rv ${RELEASE_BUILD}/doc                        ${RELEASE_FINAL}/doc
	cp ${SRC_ROOT}/obj/kernel/arch/x86_64/boot/bzImage ${RELEASE_FINAL}/

iso-root: release-final checksums
	mkdir -p ${RELEASE_BUILD}/isoroot
	cp -R --preserve ${SRC_DIR}/releng/cd_templ/* ${RELEASE_BUILD}/isoroot
	cp -R --preserve ${RELEASE_FINAL}/* ${RELEASE_BUILD}/isoroot/
	mkdir -p ${RELEASE_BUILD}/isoroot/images
	mv ${RELEASE_BUILD}/isoroot/*.img ${RELEASE_BUILD}/isoroot/images/

iso-image: iso-root release-final
	make -C ${SRC_ROOT}/obj/kernel isoimage
	cp ${SRC_ROOT}/obj/kernel/arch/x86/boot/isoimage/isolinux.bin ${RELEASE_BUILD}/isoroot/
	genisoimage -l -J -r -o ${RELEASE_FINAL}/beastix.iso -b isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table ${RELEASE_BUILD}/isoroot
	isohybrid ${RELEASE_FINAL}/beastix.iso

blank-hdimage:
	@echo Currently not implemented

clean: clean-build clean-final
