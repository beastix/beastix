#!/bin/bash

# Makes a snapshot suitable for hosting on the website
# Intended only for use with the -CURRENT branch

#cp -Rv release Beastix-CURRENT-0.1-`date +%Y-%d-%m`
cd Beastix-CURRENT-0.1-`date +%Y-%d-%m`

echo "<html><body><h1>Directory listing</h1><hr/><pre>" >index.html
for MYFILE in $(ls .)
do
  echo "<a href=\"${MYFILE}\">${MYFILE}</a>" | grep -v "index.html" >>index.html
done
echo "</pre></body></html>" >>index.html

