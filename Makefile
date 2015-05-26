include build-config.mk
include kernel/kernel.mk
include bootstrap/bootstrap.mk
include world/world.mk

bootstrap: bootstrap-binutils bootstrap-musl bootstrap-gcc bootstrap-syslinux

buildworld: buildworld-kernel-headers buildworld-musl buildworld-binutils buildworld-gcc buildworld-make buildworld-busybox buildworld-rootfs buildworld-syslinux

installworld: installworld-musl installworld-binutils installworld-gcc installworld-make installworld-rootfs installworld-syslinux

installer: build-installer

clean: clean-kernel clean-bootstrap clean-world
