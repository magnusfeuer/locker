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
#include <sys/wait.h>

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

long loopdev_setup_device(int fd, off_t offset, size_t size, char* device_result)
{

    int loopdev_fd = -1;
    struct loop_info64 info;
    int loopctl_fd = -1;
    char loopdev[128];

    sprintf(loopdev, "/dev/loop%ld", loopdev_get_free());

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
    info.lo_flags = LO_FLAGS_AUTOCLEAR | LO_FLAGS_READ_ONLY; // Free loopback on lat close
    info.lo_sizelimit = size;
    info.lo_encrypt_type = 0;

    if (ioctl(loopdev_fd, LOOP_SET_STATUS64, &info)) {
        fprintf(stderr, "Failed to set info: %s\n", strerror(errno));
        ioctl(loopdev_fd, LOOP_CLR_FD, 0);
        close(loopdev_fd);
        return -1;
    }

    strcpy(device_result, loopdev);
    return 0;
}

int main(int argc, char *const argv[], char *const* envp)
{
    int self_fd = -1;
    off_t offset = 0;
    ssize_t rd_res = 0;
    char* exec_file = 0;
    ssize_t data_sz =0;
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
    // END_OF_LOCKER:EXEC_FILE\0
    //
    // END_OF_LOCKER is the offset into argv[0] where the locker loader (this porgram)
    // ends and the native binary to execute starts.
    // EXEC_FILE is the executable, inside the loopback fs, to execute.
    // The block is null-terminated.
    offset = lseek(self_fd, -END_BLOCK_SIZE, SEEK_END);
    // Read string with end-of-elf location
    // Read the lnast END_BLOCK_SIZE bytes with the END_OF_LOCKER:END_OF_BINARY ascii fields.
    read(self_fd, end_of_locker_str, END_BLOCK_SIZE);

    end_of_locker = strtoull(end_of_locker_str, 0, 0);

    // Find colon and break into two strings, one for END_OF_LOCKER
    // and one for END_OF_BINARY
    exec_file = strchr(end_of_locker_str, ':');

    if (!exec_file) {
        fprintf(stderr, "%s has illegal locker format. Could not read END_OF_LOCKER:EXEC_FILE at the last %d bytes of file\n",
                argv[0], END_BLOCK_SIZE);
        exit(255);
    }
    *exec_file++ = 0;

    // exec_file is space-padded. Replace with null
    if (strchr(exec_file, ' '))
        *strchr(exec_file, ' ') = 0;


    // Calculate the number of bytes between the end of the ELF binary
    // and the beginning of the data field at
    // the end of the file.
    data_sz = offset - end_of_locker;

    srand(time(0));
    sprintf(mount_point, "/tmp/locker_%x", rand());

    // Setup the loopback device
    loopdev_setup_device(self_fd, end_of_locker, data_sz, loop_device);
    //strcpy(options, "");
    // sprintf(options, "loop=%s,offset=%lu,sizelimit=%lu", loop_device, end_of_locker, data_sz);
    //sprintf(options, "loop=%s", loop_device);

    if (mkdir(mount_point, 0755) == -1) {
        perror(mount_point);
        exit(255);
    }


    // Can't get the call below to work. Permission denied every time.
//    if( mount (loop_device, mount_point, "squashfs", 0, options) == -1) {
//        perror("mount");
//        exit(255);
//    }

    // Ugly workaround
    int mount_res = -1;
    if (!fork()) {
        execl("/bin/mount", "/bin/mount", "-oro", loop_device, mount_point, (char*) 0);
        perror("/bin/mount");
        exit(255);
    }

    wait(&mount_res);
    if (mount_res != 0) {
        fprintf(stderr, "Could not mount %s on %s\n", loop_device, mount_point);
        exit(255);
    }

    if (chdir(mount_point) == -1) {
        perror("chdir");
        exit(255);
    }

    // Chroot into mount point.
    if (chroot(".") == -1) {
        perror("chroot");
        exit(255);
    }

    char *nargv[argc+1];

    nargv[0] = exec_file;
    for(int i = 1; i <= argc; ++i)
        nargv[i] = argv[i];

    // Do a delayed umount on the squashfs system,
    // effectively hiding it from the rest of the system.
    if (umount2(".", MNT_DETACH) == -1) {
        perror("umount");
        exit(255);
    }
    execve(nargv[0], nargv, envp);
    perror("fexecve");
    exit(255);
}
