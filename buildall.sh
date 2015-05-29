#!/bin/bash
make -i clean
make buildkernel
make bootstrap
make buildworld
make build-installer
pushd releng
make release-final
