#!/bin/sh

# This script is used for doing updates to rootfs/ from external sources that are not appropriate to add to the local tree

wget "https://svnweb.freebsd.org/base/head/etc/services?view=co" -O - > rootfs/etc/services
