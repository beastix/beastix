#!/bin/sh

# Makes a snapshot suitable for hosting on the website
# Intended only for use with the -CURRENT branch

cp -Rv release Beastix-CURRENT-0.1-`date +%Y-%d-%m`
