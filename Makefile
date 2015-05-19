buildworld:
	make -C bin/ build
	make -C lib/ build

buildkernel:
	pushd kernel/
	make mrproper
	make defconfig
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" .config
	make bzImage
	popd
