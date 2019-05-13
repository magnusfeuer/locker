#!/bin/bash
#
# (C) 2019 Magnus Feuer
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

LOCKER_LOADER_BIN=./locknload
# Must match the #define in locker.c
END_BLOCK_SIZE=128

if [ "$#" != 3 ]
then
    echo "Usage: $0 input-native-binary output-locker-binary directory"
    echo "  native-binary - The binary to execute inside locker."
    echo "  locker-binary - The generated locker binary containing native binary and extra data."
    echo "  directory - Directory structure to be made available to native binary inside locker."
    exit 255
fi

NATIVE_INPUT_BIN=${1}
LOCKER_OUTPUT_BIN=${2}
DIRECTORY=${3}
SQUASHFS_IMG=${PWD}/squashfs.img

# Calculate the size of the locker loading binary
LOCKER_LOADER_SIZE=$(ls -l ${LOCKER_LOADER_BIN} | cut -d' ' -f 5)

# Create a new locker binary, starting with the locker loader.
cp ${LOCKER_LOADER_BIN} ${LOCKER_OUTPUT_BIN}

rm -f ${SQUASHFS_IMG}
(cd ${DIRECTORY}; mksquashfs . ${SQUASHFS_IMG})

# Add native binary
mksquashfs ${NATIVE_INPUT_BIN} ${SQUASHFS_IMG}

# Add the extra data (loopback fs) at the end of the locker binary
cat ${SQUASHFS_IMG} >> ${LOCKER_OUTPUT_BIN}

rm ${SQUASHFS_IMG}

# Create the filename that the native bin has inside the squashfs fs.
NATIVE_INPUT_BIN_IMGNAME=/$(basename ${NATIVE_INPUT_BIN})

# Setup the END_OF_LOCKER:END_OF_BINARY offset block at the end of the
# new locker binary.
printf "%-${END_BLOCK_SIZE}s" $(printf "%d:%s" ${LOCKER_LOADER_SIZE} ${NATIVE_INPUT_BIN_IMGNAME}) >> ${LOCKER_OUTPUT_BIN}

chmod 755 ${LOCKER_OUTPUT_BIN}
