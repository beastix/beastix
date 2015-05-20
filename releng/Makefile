include release_config.mk

initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
	rm tmproot/linuxrc
	cd tmproot/; find . | cpio -H newc -o | gzip > ../release/rootfs.cpio.gz; tar cvf ../release/rootfs.tar ./*; gzip ../release/rootfs.tar
	rm -rf tmproot

src:
	# TODO - make this respect the branches defined in release_config.mk
	svn co https://github.com/beastix/beastix release/src
	cd release; tar cvf src.tar src/*; gzip src.tar;
	rm -rf release/src

release: initrd src
	cp ../obj/kernel/arch/x86_64/boot/bzImage release/
	mv release/rootfs.tar.gz release/base.tgz
	mv release/rootfs.cpio.gz release/initrd_live.img
