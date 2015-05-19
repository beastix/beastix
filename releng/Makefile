initrd:
	mkdir tmproot
	cp -R --preserve ../rootfs/* tmproot/
	cp -R --preserve ../obj/bin/busybox/_install/* tmproot/
	cd tmproot/; find . | cpio -H newc -o | gzip > ../release/rootfs.cpio.gz; tar cvf ../release/rootfs.tar ./*; bzip2 ../release/rootfs.tar
	rm -rf tmproot

iso: initrd
	cd ../obj/kernel/; sed -i 's/mkisofs/genisoimage/g' arch/x86/boot/Makefile; make isoimage FDINITRD=../../releng/release/rootfs.cpio.gz; cp arch/x86/boot/image.iso ../../releng/release

release: iso
