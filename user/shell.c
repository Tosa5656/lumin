#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 256
#define MAX_ARGS 16

static void print(const char *s)
{
    write(1, s, strlen(s));
}

static void println(const char *s)
{
    print(s);
    write(1, "\n", 1);
}

static int readline(char *buf, int max)
{
    int idx = 0;
    for (;;)
    {
        char c;
        int n = read(0, &c, 1);
        if (n <= 0)
            continue;

        if (c == '\n')
        {
            buf[idx] = '\0';
            write(1, "\n", 1);
            return idx;
        }
        if (c == '\b' || c == 127)
        {
            if (idx > 0)
            {
                idx--;
                write(1, "\b \b", 3);
            }
            continue;
        }
        if (c < 32)
            continue;

        if (idx < max - 1)
        {
            buf[idx++] = c;
            write(1, &c, 1);
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
    (void)argc;

    if (strcmp(argv[0], "help") == 0)
    {
        println("Lumin Shell Commands:");
        println("  help      - this help");
        println("  echo      - print arguments");
        println("  exit      - exit shell");
        return 0;
    }

    if (strcmp(argv[0], "echo") == 0)
    {
        for (int i = 1; i < argc; i++)
        {
            if (i > 1) write(1, " ", 1);
            print(argv[i]);
        }
        println("");
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        println("bye!");
        exit(0);
    }

    print(argv[0]);
    println(": command not found");
    return -1;
}

int main(void)
{
    char line[LINE_MAX];
    char *argv[MAX_ARGS];

    println("Lumin Shell v0.1");

    for (;;)
    {
        print("$ ");
        int len = readline(line, LINE_MAX);
        (void)len;

        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        run_cmd(argc, argv);
    }

    return 0;
}
