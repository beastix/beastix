include release_config.mk

initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	
	# busybox
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
		
	# binutils
	cp -R --preserve ../obj/bin/binutils/bin/* tmproot/bin/
	cp -R --preserve ../obj/bin/binutils/etc/* tmproot/bin/
	cp -R --preserve ../obj/bin/binutils/lib/* tmproot/lib/
	cp -R --preserve ../obj/bin/binutils/x86_64-unknown-linux-gnu tmproot/
	cp -R --preserve ../obj/bin/binutils/include/* tmproot/usr/include/
	
	# gcc
	cp -R --preserve ../obj/bin/gcc/bin/*   tmproot/bin/
	cp -R --preserve ../obj/bin/gcc/lib/*   tmproot/lib/
	cp -R --preserve ../obj/bin/gcc/lib32/* tmproot/lib/
	cp -R --preserve ../obj/bin/gcc/lib64/* tmproot/lib/
	cp -R --preserve ../obj/bin/gcc/libexec tmproot/
	cp -R --preserve ../obj/bin/gcc/x86_64_unknown-linux-gnu/* tmproot/x86_64-unknown-linux-gnu/ 
	cp -R --preserve ../obj/bin/gcc/include/* tmproot/usr/include/
	
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
	rm -rf isoroot
