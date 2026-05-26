#include "shell.h"
#include "keyboard.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/acpi/acpi.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "mm/kmalloc.h"
#include "mm/vmm.h"
#include "proc/elf.h"
#include "proc/task.h"
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
            // nothing :>
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
    println("  exec <path>        - execute ELF program");
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


static void cmd_exec(char *path)
{
    if (!path) { println("usage: exec <path>"); return; }

    char rbuf[VFS_PATH_MAX];
    path = (char *)resolve_path(path, rbuf, sizeof(rbuf));

    struct vfs_file *f = vfs_open(path, VFS_O_READ);
    if (!f)
    {
        print("exec: "); print(path); println(": open failed");
        return;
    }

    uint64_t *pml4 = vmm_create_pml4();
    if (!pml4)
    {
        println("exec: vmm_create_pml4 failed");
        vfs_close(f);
        return;
    }

    uint64_t entry = elf_load(f, pml4);
    vfs_close(f);

    if (!entry)
    {
        println("exec: elf_load failed");
        return;
    }

    serial_printf("exec: starting 0x%p\n", (void*)entry);
    exec_user(entry, pml4);
    serial_printf("exec: process exited\n");
}

static int shell_tab_complete(char *buf, int *pos, int len, int max)
{
    int cursor = *pos;
    int idx = len;

    int word_start = cursor;
    while (word_start > 0 && buf[word_start - 1] != ' ' && buf[word_start - 1] != '\t')
        word_start--;

    int word_len = cursor - word_start;

    char word[VFS_PATH_MAX];
    int i;
    for (i = 0; i < word_len && i < VFS_PATH_MAX - 1; i++)
        word[i] = buf[word_start + i];
    word[i] = '\0';

    char dir[VFS_PATH_MAX];
    char prefix[VFS_NAME_MAX];
    int prefix_len;

    const char *last_slash = 0;
    for (const char *p = word; *p; p++)
        if (*p == '/') last_slash = p;

    if (last_slash)
    {
        int dir_len = last_slash - word + 1;
        int j;
        for (j = 0; j < dir_len && j < VFS_PATH_MAX - 1; j++)
            dir[j] = word[j];
        dir[j] = '\0';
        prefix_len = 0;
        const char *src = last_slash + 1;
        while (*src && prefix_len < VFS_NAME_MAX - 1)
            prefix[prefix_len++] = *src++;
        prefix[prefix_len] = '\0';

        char rbuf[VFS_PATH_MAX];
        const char *resolved = resolve_path(dir, rbuf, sizeof(rbuf));
        for (j = 0; resolved[j] && j < VFS_PATH_MAX - 1; j++)
            dir[j] = resolved[j];
        dir[j] = '\0';
    }
    else
    {
        int j;
        for (j = 0; CWD[j] && j < VFS_PATH_MAX - 1; j++)
            dir[j] = CWD[j];
        dir[j] = '\0';
        prefix_len = word_len;
        for (j = 0; j < word_len && j < VFS_NAME_MAX - 1; j++)
            prefix[j] = word[j];
        prefix[j] = '\0';
    }

    struct vfs_dentry de;
    int match_count = 0;
    char match_name[VFS_NAME_MAX];
    int is_dir = 0;

    for (int i = 0; vfs_readdir(dir, i, &de) == 0; i++)
    {
        int ok = 1;
        for (int j = 0; j < prefix_len; j++)
            if (de.name[j] != prefix[j]) { ok = 0; break; }
        if (!ok) continue;

        match_count++;
        if (match_count == 1)
        {
            int k;
            for (k = 0; de.name[k] && k < VFS_NAME_MAX - 1; k++)
                match_name[k] = de.name[k];
            match_name[k] = '\0';
            is_dir = (de.type == VFS_DIR);
        }
    }

    if (match_count == 0)
        return 0;

    int suffix_len = 0;
    while (match_name[prefix_len + suffix_len])
        suffix_len++;

    int extra = is_dir ? 1 : 0;

    if (word_start + word_len + suffix_len + extra >= max - 1)
        return 0;

    for (int i = idx; i >= word_start + word_len; i--)
        buf[i + suffix_len + extra] = buf[i];

    for (int i = 0; i < suffix_len; i++)
        buf[word_start + word_len + i] = match_name[prefix_len + i];

    if (is_dir)
        buf[word_start + word_len + suffix_len] = '/';

    *pos = word_start + word_len + suffix_len + extra;
    return suffix_len + extra;
}


void shell_run(void)
{
    char line[LINE_MAX];
    char *argv[MAX_ARGS];

    keyboard_set_tab_complete(shell_tab_complete);

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
    else if (strcmp(argv[0], "exec") == 0)
        cmd_exec(argv[1]);
    else if (strcmp(argv[0], "shutdown") == 0)
        acpi_shutdown();
    else if (strcmp(argv[0], "reboot") == 0)
        acpi_reboot();
        else
        {
            print(argv[0]);
            println(": command not found");
        }
    }
}
