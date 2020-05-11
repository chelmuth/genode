#!/bin/bash
#
# Run this script like follows.
#
#   parallel -j 30 netcat.sh -- $(seq 30)
#
# You need to install 'moreutils' on Ubuntu.

for i in $(seq 10000); do
	dd if=/dev/zero status=none count=4 | netcat 10.0.2.55 8899 > /dev/null
done
