include release_config.mk

initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
	rm tmproot/linuxrc
	cd tmproot/; find . | cpio -H newc -o | gzip > ../release/rootfs.cpio.gz; tar cvf ../release/rootfs.tar ./*; gzip ../release/rootfs.tar
	rm -rf tmproot

src:
	git clone .. release/src
	cd release; tar cvf src.tar src/*; gzip src.tar;
	rm -rf release/src

release: initrd src
	cp ../obj/kernel/arch/x86_64/boot/bzImage release/
	mv release/rootfs.tar.gz release/base.tgz
	mv release/rootfs.cpio.gz release/initrd_live.img
	cd release/; md5sum * >MD5SUMS
	mkdir isoroot
	cp -Rv release/* isoroot/
	make -C ../obj/kernel isoimage
	cp ../obj/kernel/arch/x86/boot/isoimage/isolinux.bin isoroot/
	cp -Rv cd_templ/* isoroot/
	mkdir isoroot/images
	mv isoroot/*.img isoroot/images
	genisoimage -l -J -r -o release/beastix.iso -b isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table isoroot
	isohybrid release/beastix.iso
