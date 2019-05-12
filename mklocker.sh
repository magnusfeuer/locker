#!/bin/bash
#
# (C) 2019 Magnus Feuer
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

LOCKER_BIN=./locker
# Must match the #define in locker.c
END_BLOCK_SIZE=128

if [ "$#" != 3 ]
then
    echo "Usage: $0 input-native-binary output-locker-binary extra_data"
    exit 255
fi

# Calculate the size of the locker loader binary
LOCKER_SIZE=$(ls -l ${LOCKER_BIN} | cut -d' ' -f 5)

# Calculate the size of the native binary itself.
BINARY_SIZE=$(ls -l ${1} | cut -d' ' -f 5)

# Create a new locker binary, starting with the locker loader.
cp ${LOCKER_BIN} $2

# Add the native binary at the end of the locker binary
cat $1 >> $2

# Add the extra data (loopback fs) at the end of the locker binary
cat $3 >> $2

# Setup the END_OF_LOCKER:END_OF_BINARY offset block at the end of the
# new locker binary.
printf "%-${END_BLOCK_SIZE}s" $(printf "%d:%d" ${LOCKER_SIZE} $((${LOCKER_SIZE} + ${BINARY_SIZE}))) >> $2
