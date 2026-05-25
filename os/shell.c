#include "shell.h"
#include "keyboard.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/acpi/acpi.h"
#include "fs/vfs.h"
#include "mm/kmalloc.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_ARGS 16
#define LINE_MAX 256

#define CWD "/mnt"

static const char *resolve_path(const char *path, char *buf, int bufsz)
{
    if (!path)
        return CWD;
    if (path[0] == '/')
        return path;
    int n;
    for (n = 0; CWD[n] && n < bufsz - 2; n++)
        buf[n] = CWD[n];
    buf[n++] = '/';
    while (*path && n < bufsz - 1)
        buf[n++] = *path++;
    buf[n] = '\0';
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
    println("  cat <path>         - read file");
    println("  create <path>      - create empty file");
    println("  write <path> <txt> - write text to file");
    println("  rm <path>          - delete file");
    println("  mkdir <path>       - create directory");
    println("  stat <path>        - file info");
    println("  echo <text>        - echo text");
    println("  help               - this help");
    println("  reboot             - not implemented");
}

static void cmd_ls(char *path)
{
    char buf[VFS_PATH_MAX];
    if(path != NULL)
        path = (char *)resolve_path(path, buf, sizeof(buf));
    else
        path = (char *)CWD;
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
