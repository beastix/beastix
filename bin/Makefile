build-busybox:
	pushd busybox/
	make defconfig
	sed -i "s/.*CONFIG_STATIC.*/CONFIG_STATIC=y/" .config
	sed -e 's/.*CONFIG_FEATURE_HAVE_RPC.*/CONFIG_FEATURE_HAVE_RPC=n/' -i .config
	sed -e 's/.*CONFIG_FEATURE_INETD_RPC.*/CONFIG_FEATURE_INETD_RPC=n/' -i .config
	make busybox
	popd

build: build-busybox
