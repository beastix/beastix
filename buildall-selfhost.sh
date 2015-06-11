#!/bin/bash
make -i clean
make buildkernel
make bootstrap
make buildworld
make build-installer
cd releng
make -i clean
make release-final-selfhost
cd ..
