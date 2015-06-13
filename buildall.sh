#!/bin/bash
set -e
git clean -fd
make -i clean
make buildkernel
make bootstrap
make buildworld
make buildworld-syslinux
make build-installer
pushd releng
make -i clean
make release-final
popd
