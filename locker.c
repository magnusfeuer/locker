// (C) Magnus Feuer 2019
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//


// Pre-loader for self contained binary+file system.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/loop.h>

#define MIN(A,B) ((A)<(B)?(A):(B))
#define END_BLOCK_SIZE 128

long loopdev_get_free(void)
{
    int loopctl_fd = -1;
    long devnr = -1;
    loopctl_fd = open("/dev/loop-control", O_RDWR);
    if (loopctl_fd == -1)
        perror("open: /dev/loop-control");

    devnr = ioctl(loopctl_fd, LOOP_CTL_GET_FREE);
    if (devnr == -1) {
        perror("ioctl-LOOP_CTL_GET_FREE");
        exit(255);
    }

    close(loopctl_fd);

    return devnr;
}

long loopdev_setup_device(int fd, off_t offset, char* device_result)
{

    int loopdev_fd = -1;
    struct loop_info64 info;
    int loopctl_fd = -1;
    char loopdev[128];

    sprintf(loopdev, "/dev/loop%ld", loopdev_get_free());
    printf("loopdev = %s\n", loopdev);

    loopdev_fd = open(loopdev, O_RDWR);
    if (loopdev_fd == -1) {
        fprintf(stderr, "Failed to open loopdev (%s): %s\n", loopdev, strerror(errno));
        return -1;
    }

    if (ioctl(loopdev_fd, LOOP_SET_FD, fd) < 0) {
        fprintf(stderr, "Failed to set fd: %s\n", strerror(errno));
        close(loopdev_fd);
        return -1;
    }

    memset(&info, 0, sizeof(struct loop_info64)); /* Is this necessary? */
    info.lo_offset = offset;
    info.lo_flags = LO_FLAGS_AUTOCLEAR; // Free loopback on lat close
    /* info.lo_sizelimit = 0 => max avilable */
    /* info.lo_encrypt_type = 0 => none */

    if (ioctl(loopdev_fd, LOOP_SET_STATUS64, &info)) {
        fprintf(stderr, "Failed to set info: %s\n", strerror(errno));
        ioctl(loopdev_fd, LOOP_CLR_FD, 0);
        close(loopdev_fd);
        return -1;
    }

    strcpy(device_result, loopdev);

    return 0;
}

int main(int argc, char *const argv[])
{
    int self_fd = -1;
    off_t offset = 0;
    ssize_t rd_res = 0;
    char* end_of_binary_str = 0;
    ssize_t data_sz =0;
    off_t end_of_binary = 0;
    off_t end_of_locker = 0;
    char end_of_locker_str[END_BLOCK_SIZE + 1] = { 0 };
    char loop_device[128] = { 0 };
    char mount_point[512] = { 0 };
    char options[512] = { 0 };
    uint8_t buf[0xFFFF];

    self_fd = open(argv[0], O_RDONLY);
    if (self_fd == -1)  {
        perror(argv[0]);
        exit(255);
    }

    // Position descriptor at END_BLOCK_SIZE bytes from end.
    // The final END_BLOCK_SIZE bytes has the format
    // END_OF_LOCKER:END_OF_BINARY\0
    //
    // END_OF_LOCKER is the offset into argv[0] where the locker loader (this porgram)
    // ends and the native binary to execute starts.
    // END_OF_BINARY is the offset into argv[0] where the native binary ends
    // and the data segment (file system) starts.
    // The block is null-terminated.
    offset = lseek(self_fd, -END_BLOCK_SIZE, SEEK_END);
    // Read string with end-of-elf location
    // Read the last END_BLOCK_SIZE bytes with the END_OF_LOCKER:END_OF_BINARY ascii fields.
    read(self_fd, end_of_locker_str, END_BLOCK_SIZE);

    // Find colon and break into two strings, one for END_OF_LOCKER
    // and one for END_OF_BINARY
    end_of_binary_str = strchr(end_of_locker_str, ':');
    if (!end_of_binary_str) {
        fprintf(stderr, "%s has illegal locker format. Could not read END_OF_LOCKER:END_OF_BINARY at the last %d bytes of file\n",
                argv[0], END_BLOCK_SIZE);
        exit(255);
    }
    *end_of_binary_str++ = 0;

    end_of_locker = strtoull(end_of_locker_str, 0, 0);
    end_of_binary = strtoull(end_of_binary_str, 0, 0);

    // Calculate the number of bytes between the end of the ELF binary
    // and the beginning of the end_of_elf 32-byte data field at
    // the end of the file.
    data_sz = offset - end_of_binary;

    sprintf(mount_point, "%lX", time(0));

    loopdev_setup_device(self_fd, end_of_binary, loop_device);
    //sprintf(options, "loop=%s,offset=%lu,sizelimit=%lu", end_of_binary,loop_device, data_sz);
    sprintf(options, "ro,sizelimit=%lu",  data_sz);

    printf("Mounting %s:%lu on %s. Options: %s\n", argv[0], end_of_binary, mount_point, options);
    if (mkdir(mount_point, 0755) == -1) {
        perror(mount_point);
        exit(255);
    }


    if( mount (loop_device, mount_point, "ext2", 0, options) == -1) {
        perror("mount");
        exit(255);
    }

    // Chroot into mount point.
    chroot(mount_point);

    // Jump to start of native-binary
    lseek(self_fd, end_of_locker, SEEK_SET);

    // Exec from file descriptor.
    extern char **environ;
    fexecve(self_fd, argv, environ);
    perror("fexecve");
    exit(255);
}
