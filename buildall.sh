#!/bin/bash
make -i clean
make buildkernel
make bootstrap
make buildworld
pushd releng
make release-final
