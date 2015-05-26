build-installer:
	mkdir -p ${SRC_ROOT}/installer/_install
	mkdir -p ${SRC_ROOT}/installer/_install/bin
	mkdir -p ${SRC_ROOT}/installer/_install/etc
	cp ${SRC_ROOT}/installer/rc.local ${SRC_ROOT}/installer/_install/etc/
	cp ${SRC_ROOT}/installer/installer_stage3.sh ${SRC_ROOT}/installer/_install/bin/
