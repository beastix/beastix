buildkernel:
	make -C kernel/ mrproper
	make -C kernel defconfig O=../obj/kernel
	sed "s/.*CONFIG_DEFAULT_HOSTNAME.*/CONFIG_DEFAULT_HOSTNAME=\"beastix\"/" obj/kernel/.config >obj/kernel/.config.new
	rm obj/kernel/.config
	mv obj/kernel/.config.new obj/kernel/.config
	make -C kernel bzImage O=../obj/kernel/ 

installkernel:
	cp ${SRC_ROOT}/obj/kernel/arch/x86_64/boot/bzImage /boot/bzImage

clean-kernel:
	make -C kernel/ mrproper
	make -C kernel/ clean O=../obj/kernel
