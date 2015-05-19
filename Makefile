buildworld:
	make -C bin/ build
	make -C lib/ build

buildkernel:
	export KBUILD_OUTPUT=../obj/
	make -C kernel/ O=../obj/ mrproper
	make -C kernel/ O=../obj/ defconfig
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" obj/.config
	make -C obj/ bzImage
