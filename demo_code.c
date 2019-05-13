// (C) Magnus Feuer 2019
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//


#include <stdio.h>
int main(int argc, char *const argv[], char *const* envp)
{
    FILE *in = fopen("README.md", "r");
    size_t read_res = 0;
    char buf[512];

    puts("Opening README.md from locker filesystem and dumping it on stdout!\n----");
    while((read_res = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, read_res, stdout);

    puts("----\nDone");
}
