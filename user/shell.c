#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define LINE_MAX 256
#define MAX_ARGS 16
#define HISTORY_MAX 64

static void print(const char *s)
{
    write(1, s, strlen(s));
}

static void println(const char *s)
{
    print(s);
    write(1, "\n", 1);
}

static int redraw_prev_len = 0;

static void redraw_line(const char *buf, int idx, int cursor)
{
    write(1, "\r$ ", 3);
    write(1, buf, idx);

    int clear = redraw_prev_len - idx;
    if (clear > 0) {
        for (int i = 0; i < clear; i++)
            write(1, " ", 1);
    } else {
        clear = 0;
    }

    int move = (idx + clear) - cursor;
    for (int i = 0; i < move; i++)
        write(1, "\b", 1);

    redraw_prev_len = idx;
}

static int hist_strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int readline(char *buf, int max)
{
    static char history[HISTORY_MAX][LINE_MAX];
    static int hist_count = 0;

    char saved[LINE_MAX];
    int saved_len = 0;
    int browsing = -1;
    int idx = 0;
    int cursor = 0;
    redraw_prev_len = 0;

    for (;;)
    {
        unsigned char c;
        int n = read(0, &c, 1);
        if (n <= 0)
            continue;

        if (c == '\n')
        {
            buf[idx] = '\0';
            write(1, "\n", 1);
            if (idx > 0 && (hist_count == 0 ||
                hist_strcmp(history[hist_count - 1], buf) != 0))
            {
                if (hist_count == HISTORY_MAX)
                {
                    for (int i = 1; i < HISTORY_MAX; i++)
                        for (int j = 0; j < LINE_MAX; j++)
                            history[i - 1][j] = history[i][j];
                    hist_count--;
                }
                int i;
                for (i = 0; i < idx && i < LINE_MAX - 1; i++)
                    history[hist_count][i] = buf[i];
                history[hist_count][i] = '\0';
                hist_count++;
            }
            return idx;
        }

        if (c >= 0x80)
        {
            switch (c)
            {
                case 0x80: /* KEY_UP */
                {
                    if (hist_count == 0) continue;
                    if (browsing == -1)
                    {
                        int i;
                        for (i = 0; i < idx && i < LINE_MAX - 1; i++)
                            saved[i] = buf[i];
                        saved[idx] = '\0';
                        saved_len = idx;
                        browsing = hist_count - 1;
                    }
                    else if (browsing > 0)
                    {
                        browsing--;
                    }
                    else
                    {
                        continue;
                    }
                    const char *h = history[browsing];
                    idx = 0;
                    while (h[idx] && idx < max - 1)
                    {
                        buf[idx] = h[idx];
                        idx++;
                    }
                    cursor = idx;
                    redraw_line(buf, idx, cursor);
                    continue;
                }

                case 0x81: /* KEY_DOWN */
                {
                    if (browsing == -1) continue;
                    if (browsing < hist_count - 1)
                    {
                        browsing++;
                        const char *h = history[browsing];
                        idx = 0;
                        while (h[idx] && idx < max - 1)
                        {
                            buf[idx] = h[idx];
                            idx++;
                        }
                    }
                    else
                    {
                        browsing = -1;
                        idx = 0;
                        while (saved[idx] && idx < max - 1)
                        {
                            buf[idx] = saved[idx];
                            idx++;
                        }
                    }
                    cursor = idx;
                    redraw_line(buf, idx, cursor);
                    continue;
                }

                case 0x82: /* KEY_LEFT */
                {
                    if (cursor > 0)
                    {
                        cursor--;
                        redraw_line(buf, idx, cursor);
                    }
                    continue;
                }

                case 0x83: /* KEY_RIGHT */
                {
                    if (cursor < idx)
                    {
                        cursor++;
                        redraw_line(buf, idx, cursor);
                    }
                    continue;
                }

                case 0x84: /* KEY_HOME */
                {
                    cursor = 0;
                    redraw_line(buf, idx, cursor);
                    continue;
                }

                case 0x85: /* KEY_END */
                {
                    cursor = idx;
                    redraw_line(buf, idx, cursor);
                    continue;
                }

                case 0x86: /* KEY_DEL */
                {
                    if (cursor < idx)
                    {
                        for (int i = cursor; i < idx; i++)
                            buf[i] = buf[i + 1];
                        idx--;
                        redraw_line(buf, idx, cursor);
                    }
                    continue;
                }

                default:
                    continue;
            }
        }

        if (c == 0x1B)
        {
            char seq[2];
            if (read(0, &seq[0], 1) <= 0) continue;
            if (read(0, &seq[1], 1) <= 0) continue;

            if (seq[0] == '[')
            {
                switch (seq[1])
                {
                    case 'A': /* Up */
                    {
                        if (hist_count == 0) continue;
                        if (browsing == -1)
                        {
                            int i;
                            for (i = 0; i < idx && i < LINE_MAX - 1; i++)
                                saved[i] = buf[i];
                            saved[idx] = '\0';
                            saved_len = idx;
                            browsing = hist_count - 1;
                        }
                        else if (browsing > 0)
                        {
                            browsing--;
                        }
                        else
                        {
                            continue;
                        }
                        const char *h = history[browsing];
                        idx = 0;
                        while (h[idx] && idx < max - 1)
                        {
                            buf[idx] = h[idx];
                            idx++;
                        }
                        cursor = idx;
                        redraw_line(buf, idx, cursor);
                        continue;
                    }

                    case 'B': /* Down */
                    {
                        if (browsing == -1) continue;
                        if (browsing < hist_count - 1)
                        {
                            browsing++;
                            const char *h = history[browsing];
                            idx = 0;
                            while (h[idx] && idx < max - 1)
                            {
                                buf[idx] = h[idx];
                                idx++;
                            }
                        }
                        else
                        {
                            browsing = -1;
                            idx = 0;
                            while (saved[idx] && idx < max - 1)
                            {
                                buf[idx] = saved[idx];
                                idx++;
                            }
                        }
                        cursor = idx;
                        redraw_line(buf, idx, cursor);
                        continue;
                    }

                    case 'C': /* Right */
                    {
                        if (cursor < idx)
                        {
                            cursor++;
                            redraw_line(buf, idx, cursor);
                        }
                        continue;
                    }

                    case 'D': /* Left */
                    {
                        if (cursor > 0)
                        {
                            cursor--;
                            redraw_line(buf, idx, cursor);
                        }
                        continue;
                    }

                    case 'H': /* Home */
                    {
                        cursor = 0;
                        redraw_line(buf, idx, cursor);
                        continue;
                    }

                    case 'F': /* End */
                    {
                        cursor = idx;
                        redraw_line(buf, idx, cursor);
                        continue;
                    }

                    case '3': /* Del (ESC[3~) */
                    {
                        char tilde;
                        if (read(0, &tilde, 1) > 0 && tilde == '~')
                        {
                            if (cursor < idx)
                            {
                                for (int i = cursor; i < idx; i++)
                                    buf[i] = buf[i + 1];
                                idx--;
                                redraw_line(buf, idx, cursor);
                            }
                        }
                        continue;
                    }

                    default:
                    {
                        char d = seq[1];
                        while (read(0, &d, 1) > 0 &&
                               !((d >= 'A' && d <= 'Z') ||
                                 (d >= 'a' && d <= 'z') ||
                                 d == '~'))
                            ;
                        continue;
                    }
                }
            }
            else
            {
                char d = seq[1];
                if (!((d >= 'A' && d <= 'Z') ||
                      (d >= 'a' && d <= 'z')))
                {
                    while (read(0, &d, 1) > 0 &&
                           !((d >= 'A' && d <= 'Z') ||
                             (d >= 'a' && d <= 'z') ||
                             d == '~'))
                        ;
                }
            }
            continue;
        }

        if (c == '\b' || c == 127)
        {
            if (cursor > 0)
            {
                for (int i = cursor; i < idx; i++)
                    buf[i - 1] = buf[i];
                idx--;
                cursor--;
                redraw_line(buf, idx, cursor);
            }
            continue;
        }

        if (c < 32)
            continue;

        if (idx < max - 1)
        {
            for (int i = idx; i > cursor; i--)
                buf[i] = buf[i - 1];
            buf[cursor] = c;
            idx++;
            cursor++;
            redraw_line(buf, idx, cursor);
        }
    }
}

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    while (*line)
    {
        while (*line == ' ')
            line++;
        if (!*line)
            break;
        if (argc >= max)
            break;

        if (*line == '"')
        {
            line++;
            argv[argc++] = line;
            while (*line && *line != '"')
                line++;
            if (*line)
                *line++ = '\0';
        }
        else
        {
            argv[argc++] = line;
            while (*line && *line != ' ')
                line++;
            if (*line)
                *line++ = '\0';
        }
    }
    return argc;
}

static int run_cmd(int argc, char **argv)
{
    if (argc == 0 || argv[0] == '\0')
    {
        return 0;
    }

    if (strcmp(argv[0], "cd") == 0)
    {
        const char *target_path = (argc > 1) ? argv[1] : "/";
        if (chdir(target_path) < 0)
        {
            write(1, "cd: no such file or directory\n", 30);
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[0], "pwd") == 0)
    {
        char cwd_buf[256];
        if (getcwd(cwd_buf, sizeof(cwd_buf)) == 0)
        {
            int len = 0;
            while (cwd_buf[len] != '\0') len++;
            
            write(1, cwd_buf, len);
            write(1, "\n", 1);
        }
        else
        {
            write(1, "pwd: error getting current directory\n", 37);
        }
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        syscall(SYS_exit, 0, 0, 0); 
        return 0; 
    }

    if (strcmp(argv[0], "export") == 0)
    {
        if (argc < 2)
        {
            write(1, "export: usage: export VAR=VALUE\n", 32);
        }
        else
        {
            write(1, "export: environment variables not supported yet\n", 48);
        }
        return 0;
    }

    char buffer[256];
    
    int cmd_len = 0;
    while (argv[0][cmd_len] != '\0') cmd_len++;
    if (cmd_len > 200) return -1;

    strcpy(buffer, "/system/bin/");
    strcat(buffer, argv[0]);
    strcat(buffer, ".elf");

    int pid = spawn(buffer, argc, argv);
    if (pid < 0)
    {
        write(1, "shell: command not found\n", 25);
        return -1;
    }

    return waitpid(pid);
}

int main(void)
{
    char line[LINE_MAX];
    char *argv[MAX_ARGS];

    println("Lumin Shell v0.1");

    for (;;)
    {
        print("$ ");
        readline(line, LINE_MAX);

        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        run_cmd(argc, argv);
    }

    return 0;
}
