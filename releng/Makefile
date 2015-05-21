include release_config.mk

initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	
	# busybox
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
		
	# binutils
	cp -R --preserve ../obj/bin/binutils/_install/* tmproot/
	
	# gcc
	cp -R --preserve ../obj/bin/gcc/_install/* tmproot/
	
	# musl
	cp -R --preserve ../obj/lib/musl/_install/* tmproot/
	
	# make
	cp -R --preserve ../obj/bin/make/_install/* tmproot/
	
	rm tmproot/linuxrc
	cd tmproot/; find . | cpio -H newc -o | gzip > ../release/rootfs.cpio.gz; tar cvf ../release/rootfs.tar ./*; gzip ../release/rootfs.tar
	rm -rf tmproot

src:
	git clone .. release/src
	cd release; tar cvf src.tar src/*; gzip src.tar;
	rm -rf release/src

ports:

release: initrd src ports
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
	rm -rf isoroot
