buildworld:
	make -C bin/ build
	make -C lib/ build

buildkernel:
	export KBUILD_OUTPUT=../obj/
	make -C kernel/ mrproper
	make -C kernel/ defconfig
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" kernel/.config
	make -C kernel/ O=../obj/ bzImage
