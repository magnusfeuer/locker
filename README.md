# LOCKER

**Thanks to [Tony Rogvall](https://github.com/tonyrog) for coming up with the original idea in an Erlang context.**

Pack up a file system with any binary and run it anywhere with no
local dependencies.

Locker provides tooling to containerize any binary so that it executes
inside a file system that is embedded into the binary itself.

The core trick is that an ELF binary file can have any kind of data
tagged on to the end of it without impacting the execution itself.

We use this to create a locker loader `locknload` binary, and then add a squashfs file system
to the end of the binary.

`locknload`, when executed, will mount the squashfs, chroot into it, and execute the target
binary from inside the chroot jail. The target binary will have access to all files
in stored in the squashfs fs, and nothing else.

The locker binary format is:

Section   | Description
----------|---------
locknload | Standard EFL binary compiled from locknload.c
squashfs  | A squashfs image generated by ./mklocker.sh (see below)
end block | a 128 byte area containing execution info. (See below)

## End block
The end block occupies the last 128 bytes of the locker binary and has the following format:

Section         | Description
--------------- |---------
locknload size  | The size, as an integer string, of the locknload ELF binary.
target binary   | Path to the target binary, inside the squashfs, to execute once chroot has completed.
padding         | Space characters to fill out to 128 bytes.


# Demo

The demo showcases how we can package up `demo_code`, which prints
`README.md` (this file), into a locker binary and run it.

First build locknload and the demo code:

    make locknload demo_code


Create a directory which we will package up as a squashfs image in the generated locker binary.

    mkdir fs

Copy in the `README.md` file that `demo_code` will print to stdout.

    cp README.md fs

Create a locker binary, `demo_code.locker` that runs `demo_code` code,
and package it up with a squashfs image generated from `fs`.

    ./mklocker.sh demo_code demo_code.locker fs

Move the resulting demo code to /tmp, where README.md is not accessible, and run it from that directory.

    mv demo_code.locker /tmp
    cd /tmp
    sudo ./demo_code.locker

# TO BE FIXED
Can we use FUSE virtual file system instead of loopback fs so that we
don't have to run as root?
