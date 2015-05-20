buildworld:
	make -C bin/ build
	make -C lib/ build
	./update_rootfs.sh

buildkernel:
	export KBUILD_OUTPUT=../obj/kernel/
	make -C kernel/ O=../obj/kernel/ mrproper
	make -C kernel/ O=../obj/kernel/ defconfig
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" obj/kernel/.config
	make -C obj/kernel/ bzImage
