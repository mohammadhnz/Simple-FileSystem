#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LibFS.h"

void usage(char *prog) {
    printf("USAGE: %s <disk_image_file>\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) usage(argv[0]);

    if (FS_Boot(argv[1]) < 0) {
        printf("ERROR: can't boot file system from file '%s'\n", argv[1]);
        return -1;
    } else printf("file system booted from file '%s' \033[0m\n", argv[1]);
    char *fn;

    printf("\n\033[0;31mTEST 0: \033[0m\n");
    fn = "/first-file";
    if (File_Create(fn) < 0) printf("ERROR: can't create file '%s'\n", fn);
    else printf("\033[32;1mfile '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 1: \033[0m\n");
    fn = "/second-file";
    if (File_Create(fn) < 0) printf("ERROR: can't create file '%s'\n", fn);
    else printf("\033[32;1mfile '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 2: \n\033[0m");
    fn = "/first-dir";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 3: \n\033[0m");
    fn = "/first-dir/second-dir";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 4: \n\033[0m");
    fn = "/first-file/second-dir";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 5: \n\033[0m");
    fn = "/first_dir/third*dir";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 6: \n\033[0m");
    fn = "/first-file";
    if (File_Unlink(fn) < 0) printf("ERROR: can't unlink file '%s'\n", fn);
    else printf("\033[32;1mfile '%s' unlinked successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 7: \n\033[0m");
    fn = "/first-dir";
    if (Dir_Unlink(fn) < 0) printf("ERROR: can't unlink dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' unlinked successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 8: \n\033[0m");
    fn = "/first-dir/second-dir";
    if (Dir_Unlink(fn) < 0) printf(
            "\033[32;1mERROR: can't unlink dir: err: %d '%s'\033[0m\n", osErrno,fn
            );
    else printf("mdir '%s' unlinked successfully \033[32;1", fn);

    printf("\n\033[0;31mTEST 9: \n\033[0m");
    fn = "/second-file";
    int fd = File_Open(fn);
    if (fd < 0) printf("ERROR: can't open file '%s'\n", fn);
    else printf("\033[32;1mfile '%s' opened successfully, fd=%d \033[0m\n", fn, fd);

    printf("\n\033[0;31mTEST 10: \n\033[0m");
    char buf[1024];
    char *ptr = buf;
    for (int i = 0; i < 1000; i++) {
        sprintf(ptr, "%d %s", i, (i + 1) % 10 == 0 ? "\n" : "");
        ptr += strlen(ptr);
        if (ptr >= buf + 1000) break;
    }
    if (File_Write(fd, buf, 1024) != 1024)
        printf("ERROR: can't write 1024 bytes to fd=%d\n", fd);
    else printf("\033[32;1msuccessfully wrote 1024 bytes to fd=%d \033[0m\n", fd);

    printf("\n\033[0;31mTEST 11: \n\033[0m");
    if (File_Close(fd) < 0) printf("ERROR: can't close fd %d\n", fd);
    else printf("\033[32;1mfd %d closed successfully \033[0m\n", fd);

    printf("\n\033[0;31mTEST 12: \n\033[0m");
    if (FS_Sync() < 0) {
        printf("ERROR: can't sync file system to file '%s'\n", argv[1]);
        return -1;
    } else printf("\033[32;1mfile system sync'd to file '%s' \033[0m\n", argv[1]);
    printf("\n\033[0;31mTEST 13: leaf directory creation \n\033[0m");
    fn = "/x";
    Dir_Create(fn);
    fn = "/x/y";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 14: duplicate directory creation\n\033[0m");
    fn = "/qq";
    Dir_Create(fn);
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n", fn);

    printf("\n\033[0;31mTEST 15: recursive directory creation\n\033[0m");
    fn = "/a/b";
    if (Dir_Create(fn) < 0) printf("ERROR: can't create dir '%s'\n", fn);
    else printf("\033[32;1mdir '%s' created successfully \033[0m\n\n", fn);
    printf("\033[32;1m OK \033[0m\n");

    return 0;
}
