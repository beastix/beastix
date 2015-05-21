include release_config.mk

initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	
	# busybox
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
		
	# binutils
	cp -R --preserve ../obj/bin/binutils/_install/* tmproot/
	cp -R --preserve ../obj/bin/binutils/_install/x86_64-unknown-linux-musl/* tmproot/
	
	# gcc
	cp -R --preserve ../obj/bin/gcc/_install/* tmproot/
		
	# kernel headers
	cp -R --preserve ../obj/kernel/include/* tmproot/include/
	cd tmproot; ln -s /include usr/include
	
	# musl
	cp -R --preserve ../obj/lib/musl/_install/* tmproot/
	
	# libedit
	cp -R --preserve ../obj/lib/libedit/_install/* tmproot/
	
	# make
	cp -R --preserve ../obj/bin/make/_install/* tmproot/
	
	cd tmproot/; ln -s  /lib/libc.so bin/ldd;
	cd tmproot/; ln -sf /lib/libc.so lib/ld-musl-x86_64.so.1
	rm tmproot/linuxrc
	cd tmproot/; find . | cpio -H newc -o | gzip > ../release/rootfs.cpio.gz; tar cvf ../release/rootfs.tar ./*; gzip ../release/rootfs.tar
	rm -rf tmproot

src:
	git clone .. release/src
	cd release; tar cvf src.tar src/*; gzip src.tar;
	rm -rf release/src

ports:

hdimage:
	dd if=/dev/zero of=release/beastix.hdd bs=1M count=4096
	/sbin/mke2fs release/beastix.hdd

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
