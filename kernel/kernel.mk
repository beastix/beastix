buildkernel:
	make -C kernel/ mrproper
	make -C kernel defconfig O=../obj/kernel
	sed -i "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" obj/kernel/.config
	make -C kernel bzImage O=../obj/kernel/ 

installkernel:

clean-kernel:
	make -C kernel/ mrproper
	make -C kernel/ clean O=../obj/kernel
