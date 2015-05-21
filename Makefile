bootstrap: FORCE
	make -C bin/ build-bootstrap
	rm -rf bootstrap/tools
	mkdir bootstrap/tools
	make -C kernel/ INSTALL_HDR_PATH=../bootstrap/tools headers_install
	cp -Rv bootstrap/binutils/_install/* bootstrap/tools/
	cp -Rv bootstrap/gcc/_install/* bootstrap/tools/
	make -C lib/ build-bootstrap
	cp -Rv bootstrap/musl/_install/* bootstrap/tools/
	cd bootstrap/tools; mkdir usr; cp -Rv include usr/

FORCE:

buildworld:
	make -C lib/ build
	make -C bin/ build
	./update_rootfs.sh

buildkernel:
	export KBUILD_OUTPUT=../obj/kernel/
	make -C kernel/ O=../obj/kernel/ mrproper
	make -C kernel/ O=../obj/kernel/ defconfig
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" obj/kernel/.config
	make -C kernel/ INSTALL_HDR_PATH=../obj/kernel headers_install
	make -C obj/kernel/ bzImage

clean:
	rm -rf obj/lib/musl/*
	rm -rf obj/lib/libedit/*
	rm -rf obj/bin/binutils/*
	rm -rf obj/bin/busybox/*
	rm -rf obj/bin/gcc/*
	rm -rf obj/kernel/*
	rm -rf releng/release/*
	rm -rf bootstrap/binutils/_install
	rm -rf bootstrap/gcc/_install
