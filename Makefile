include build-config.mk
include kernel/kernel.mk
include bootstrap/bootstrap.mk
include world/world.mk

clean: clean-kernel clean-bootstrap clean-world
