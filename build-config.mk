PWD               != pwd
CFLAGS            :=
LDFLAGS           := 
BOOTSTRAP_CFLAGS  :=
BOOTSTRAP_LDFLAGS :=
BOOTSTRAP_TOOLS   := ${PWD}/bootstrap/tools
BOOTSTRAP_CONFIG  := --target=x86_64-unknown-linux-musl --prefix=${BOOTSTRAP_TOOLS} --disable-install-libbfd --with-sysroot=${BOOTSTRAP_TOOLS}
BOOTSTRAP_PATH    := ${BOOTSTRAP_TOOLS}/bin:${BOOTSTRAP_TOOLS}/x86_64-unknown-linux-musl/bin:/bin:/usr/bin
BOOTSTRAP_CC      := x86_64-unknown-linux-musl-gcc
WORLD_CC          := x86_64-unknown-linux-musl-gcc
WORLD_CONFIG      := --target=x86_64-unknown-linux-musl --host=x86_64-unknown-linux-musl --disable-shared
WORLD_CFLAGS      := -static -I${BOOTSTRAP_TOOLS}/usr/include/ ${CFLAGS}
WORLD_LDFLAGS     := -static ${LDFLAGS}
WORLD_BUILD       := ${PWD}/obj
SRC_ROOT          := ${PWD}
INSTALL_ROOT      := 
BEASTIX_VER       := 0.1-CURRENT
