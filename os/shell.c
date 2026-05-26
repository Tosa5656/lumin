#include "shell.h"
#include "keyboard.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/acpi/acpi.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "mm/kmalloc.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_ARGS 16
#define LINE_MAX 256

static char CWD[VFS_PATH_MAX] = "/";

static const char *resolve_path(const char *path, char *buf, int bufsz)
{
    if (!path) return CWD;

    int n;
    if (path[0] == '/')
    {
        n = 0;
        buf[0] = '\0';
    }
    else
    {
        for (n = 0; CWD[n] && n < bufsz - 1; n++)
            buf[n] = CWD[n];
        buf[n] = '\0';
    }

    while (*path)
    {
        while (*path == '/')
            path++;
        if (!*path) break;

        const char *end = path;
        while (*end && *end != '/')
            end++;

        int len = end - path;
        if (len == 2 && path[0] == '.' && path[1] == '.')
        {
            if (n > 1)
            {
                n--;
                while (n > 0 && buf[n] != '/')
                    n--;
                buf[n] = '\0';
            }
            if (n == 0)
            {
                buf[0] = '/';
                buf[1] = '\0';
                n = 1;
            }
        }
        else if (len == 1 && path[0] == '.')
        {
            /* skip */
        }
        else
        {
            if (n == 0 || buf[n - 1] != '/')
                buf[n++] = '/';
            for (int i = 0; i < len && n < bufsz - 1; i++)
                buf[n++] = path[i];
            buf[n] = '\0';
        }

        path = end;
    }

    if (n == 0) { buf[0] = '/'; buf[1] = '\0'; }
    return buf;
}

static void print(const char *s)
{
    vga_write(s, vga_default_color);
    serial_write(s);
}

static void println(const char *s)
{
    print(s);
    vga_putchar('\n', vga_default_color);
    serial_putchar('\n');
}

static void print_num(unsigned long long n)
{
    char tmp[32];
    int i = 0;
    if (n == 0) tmp[i++] = '0';
    while (n > 0) { tmp[i++] = '0' + n % 10; n /= 10; }
    for (int j = 0; j < i / 2; j++) { char t = tmp[j]; tmp[j] = tmp[i-1-j]; tmp[i-1-j] = t; }
    tmp[i] = '\0';
    print(tmp);
}

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    int in = 0;
    for (int i = 0; line[i] && argc < max; i++)
    {
        if (line[i] == ' ' || line[i] == '\t')
        {
            if (in) { line[i] = '\0'; in = 0; }
            continue;
        }
        if (!in) { argv[argc++] = &line[i]; in = 1; }
    }
    return argc;
}

static int strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void cmd_help(void)
{
    println("Lumin Shell v0.1");
    println("  ls [path]          - list directory");
    println("  cd [path]          - change directory");
    println("  pwd                - print working directory");
    println("  cat <path>         - read file");
    println("  create <path>      - create empty file");
    println("  write <path> <txt> - write text to file");
    println("  rm <path>          - delete file");
    println("  mkdir <path>       - create directory");
    println("  stat <path>        - file info");
    println("  echo <text>        - echo text");
    println("  mount <dev> <path> - mount device at path");
    println("  umount <path>      - unmount filesystem");
    println("  help               - this help");
    println("  shutdown           - power off");
}

static void cmd_cd(const char *path)
{
    char rbuf[VFS_PATH_MAX];
    const char *target = resolve_path(path, rbuf, sizeof(rbuf));

    struct vfs_dentry de;
    if (vfs_stat(target, &de) != 0)
    {
        print("cd: ");
        print(target);
        println(": no such file or directory");
        return;
    }
    if (de.type != VFS_DIR)
    {
        print("cd: ");
        print(target);
        println(": not a directory");
        return;
    }

    int i;
    for (i = 0; target[i] && i < VFS_PATH_MAX - 1; i++)
        CWD[i] = target[i];
    CWD[i] = '\0';
}

static void cmd_pwd(void)
{
    println(CWD);
}

static void cmd_mount(int argc, char **argv)
{
    if (argc < 2) { println("usage: mount <device> <path>"); return; }

    char devbuf[VFS_PATH_MAX];
    char mntbuf[VFS_PATH_MAX];
    const char *dev = resolve_path(argv[0], devbuf, sizeof(devbuf));
    const char *mnt = resolve_path(argv[1], mntbuf, sizeof(mntbuf));

    struct block_device *bdev = vfs_get_block_device(dev);
    if (!bdev)
    {
        print("mount: ");
        print(dev);
        println(": not a block device");
        return;
    }

    if (vfs_mount_fat32(mnt, bdev) != 0)
    {
        print("mount: ");
        print(mnt);
        println(": mount failed (not FAT32?)");
    }
}

static void cmd_umount(const char *path)
{
    if (!path) { println("usage: umount <path>"); return; }
    char buf[VFS_PATH_MAX];
    path = resolve_path(path, buf, sizeof(buf));
    if (vfs_umount(path) != 0)
    {
        print("umount: ");
        print(path);
        println(": not mounted");
    }
}

static void cmd_ls(const char *arg)
{
    char buf[VFS_PATH_MAX];
    const char *path = (arg) ? resolve_path(arg, buf, sizeof(buf)) : CWD;
    struct vfs_dentry de;
    int found = 0;
    for (int i = 0; vfs_readdir(path, i, &de) == 0; i++)
    {
        found = 1;
        print(de.name);
        if (de.type == VFS_DIR) print("/");
        print("  [");
        print_num(de.size);
        print("]\n");
        serial_write(de.name);
        serial_write(de.type == VFS_DIR ? "/" : "");
        serial_printf(" [size=%llu]\n", (unsigned long long)de.size);
    }
    if (!found)
        println("(empty)");
}

static void cmd_cat(char *path)
{
    if (!path) { println("usage: cat <path>"); return; }
    char rbuf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, rbuf, sizeof(rbuf));
    struct vfs_file *f = vfs_open(path, VFS_O_READ);
    if (!f) { println("cat: open failed"); return; }

    uint8_t buf[257];
    int r;
    while ((r = vfs_read(f, 256, buf)) > 0)
    {
        buf[r] = '\0';
        print((char *)buf);
    }
    vga_putchar('\n', vga_default_color);
    serial_putchar('\n');
    vfs_close(f);
}

static void cmd_create(char *path)
{
    if (!path) { println("usage: create <path>"); return; }
    char buf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, buf, sizeof(buf));
    if (vfs_create(path) == 0)
        serial_printf("shell: created '%s'\n", path);
    else
        serial_printf("shell: create '%s' FAILED\n", path);
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 2) { println("usage: write <path> <text>"); return; }
    char rbuf[VFS_PATH_MAX];
    char *path = (char *)resolve_path(argv[0], rbuf, sizeof(rbuf));
    char *text = argv[1];

    struct vfs_file *f = vfs_open(path, VFS_O_WRITE);
    if (!f) { println("write: open failed"); return; }

    int len = 0;
    while (text[len]) len++;
    int r = vfs_write(f, len, text);
    serial_printf("shell: wrote %d bytes to '%s'\n", r, path);
    vfs_close(f);
}

static void cmd_rm(char *path)
{
    if (!path) { println("usage: rm <path>"); return; }
    char buf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, buf, sizeof(buf));
    if (vfs_unlink(path) == 0)
        serial_printf("shell: removed '%s'\n", path);
    else
        serial_printf("shell: remove '%s' FAILED\n", path);
}

static void cmd_mkdir(char *path)
{
    if (!path) { println("usage: mkdir <path>"); return; }
    char buf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, buf, sizeof(buf));
    if (vfs_mkdir(path) == 0)
        serial_printf("shell: mkdir '%s'\n", path);
    else
        serial_printf("shell: mkdir '%s' FAILED\n", path);
}

static void cmd_stat(char *path)
{
    if (!path) { println("usage: stat <path>"); return; }
    char buf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, buf, sizeof(buf));
    struct vfs_dentry de;
    if (vfs_stat(path, &de) != 0)
    {
        println("stat: failed");
        return;
    }
    serial_printf("stat '%s': ino=%llu size=%llu type=%d name=%s\n",
                  path, (unsigned long long)de.ino,
                  (unsigned long long)de.size, de.type, de.name);
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 0; i < argc; i++)
    {
        if (i > 0) { vga_putchar(' ', vga_default_color); serial_putchar(' '); }
        print(argv[i]);
    }
    vga_putchar('\n', vga_default_color);
    serial_putchar('\n');
}

void shell_run(void)
{
    char line[LINE_MAX];
    char *argv[MAX_ARGS];

    println("Lumin Shell. Type 'help' for commands.");

    while (1)
    {
        print(CWD);
        print(" $ ");

        int len = keyboard_readline(line, LINE_MAX);
        (void)len;

        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0) continue;

        if (strcmp(argv[0], "help") == 0)
            cmd_help();
        else if (strcmp(argv[0], "ls") == 0)
            cmd_ls(argc > 1 ? argv[1] : NULL);
        else if (strcmp(argv[0], "cat") == 0)
            cmd_cat(argv[1]);
        else if (strcmp(argv[0], "create") == 0)
            cmd_create(argv[1]);
        else if (strcmp(argv[0], "write") == 0)
            cmd_write(argc - 1, argv + 1);
        else if (strcmp(argv[0], "rm") == 0)
            cmd_rm(argv[1]);
        else if (strcmp(argv[0], "mkdir") == 0)
            cmd_mkdir(argv[1]);
        else if (strcmp(argv[0], "stat") == 0)
            cmd_stat(argv[1]);
        else if (strcmp(argv[0], "cd") == 0)
            cmd_cd(argv[1]);
        else if (strcmp(argv[0], "pwd") == 0)
            cmd_pwd();
        else if (strcmp(argv[0], "mount") == 0)
            cmd_mount(argc - 1, argv + 1);
        else if (strcmp(argv[0], "umount") == 0)
            cmd_umount(argv[1]);
        else if (strcmp(argv[0], "echo") == 0)
            cmd_echo(argc - 1, argv + 1);
        else if (strcmp(argv[0], "shutdown") == 0)
            acpi_shutdown();
        else
        {
            print(argv[0]);
            println(": command not found");
        }
    }
}
