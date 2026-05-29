#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 65536
#define MAX_LINES 1024
#define LINE_MAX 256

static char buf[BUF_SIZE];
static long buf_len;
static int num_lines;
static long line_offsets[MAX_LINES];
static int line_lens[MAX_LINES];
static int cur_line;

static void rebuild_lines(void)
{
    num_lines = 0;
    long pos = 0;
    while (pos < buf_len && num_lines < MAX_LINES)
    {
        line_offsets[num_lines] = pos;
        long start = pos;
        while (pos < buf_len && buf[pos] != '\n')
            pos++;
        line_lens[num_lines] = (int)(pos - start);
        num_lines++;
        if (pos < buf_len && buf[pos] == '\n')
            pos++;
    }
    if (cur_line >= num_lines)
        cur_line = num_lines - 1;
    if (cur_line < 0)
        cur_line = 0;
}

static void print_lines(int start, int end)
{
    for (int i = start; i <= end && i < num_lines; i++)
    {
        write(1, buf + line_offsets[i], line_lens[i]);
        write(1, "\n", 1);
    }
}

static int read_file(const char *path)
{
    int fd = open(path, 0);
    if (fd < 0)
        return -1;
    buf_len = 0;
    int n;
    while ((n = read(fd, buf + buf_len, BUF_SIZE - buf_len)) > 0)
        buf_len += n;
    close(fd);
    cur_line = 0;
    rebuild_lines();
    return 0;
}

static int write_file(const char *path)
{
    int fd = open(path, 1);
    if (fd < 0)
        return -1;
    long written = 0;
    while (written < buf_len)
    {
        int n = write(fd, buf + written, buf_len - written);
        if (n <= 0)
        {
            close(fd);
            return -1;
        }
        written += n;
    }
    close(fd);
    return 0;
}

static void insert_line(int after, const char *text, int len)
{
    if (num_lines >= MAX_LINES)
        return;
    long pos;
    if (after < 0)
        pos = 0;
    else if (after >= num_lines)
        pos = buf_len;
    else
        pos = line_offsets[after + 1];
    if (buf_len + len + 1 > BUF_SIZE)
        return;
    memmove(buf + pos + len + 1, buf + pos, buf_len - pos);
    memcpy(buf + pos, text, len);
    buf[pos + len] = '\n';
    buf_len += len + 1;
    cur_line = after + 1;
    rebuild_lines();
}

static void delete_line(int idx)
{
    if (idx < 0 || idx >= num_lines)
        return;
    long pos = line_offsets[idx];
    int remove_len = line_lens[idx] + 1;
    memmove(buf + pos, buf + pos + remove_len, buf_len - pos - remove_len);
    buf_len -= remove_len;
    rebuild_lines();
}

static void read_stdin_line(char *out, int max)
{
    int i = 0;
    char c;
    while (i < max - 1)
    {
        if (read(0, &c, 1) <= 0)
            break;
        if (c == '\n')
            break;
        out[i++] = c;
    }
    out[i] = '\0';
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: ed <filename>\n");
        return 1;
    }

    if (read_file(argv[1]) < 0)
    {
        buf_len = 0;
        cur_line = 0;
        num_lines = 0;
    }

    char line[LINE_MAX];
    for (;;)
    {
        write(1, ". ", 2);
        read_stdin_line(line, LINE_MAX);
        if (line[0] == '\0')
        {
            if (cur_line >= 0 && cur_line < num_lines)
                print_lines(cur_line, cur_line);
            continue;
        }

        if (line[0] >= '0' && line[0] <= '9')
        {
            int n = 0;
            int i = 0;
            while (line[i] >= '0' && line[i] <= '9')
                n = n * 10 + (line[i++] - '0');
            if (n >= 1 && n <= num_lines)
            {
                cur_line = n - 1;
                print_lines(cur_line, cur_line);
            }
            continue;
        }

        switch (line[0])
        {
            case 'i':
            {
                char input[1024];
                int input_len = 0;
                char in_line[LINE_MAX];
                for (;;)
                {
                    read_stdin_line(in_line, LINE_MAX);
                    if (in_line[0] == '.' && in_line[1] == '\0')
                        break;
                    int l = strlen(in_line);
                    if (input_len + l + 1 > 1024)
                        break;
                    memcpy(input + input_len, in_line, l);
                    input_len += l;
                    input[input_len++] = '\n';
                }
                if (input_len > 0)
                    insert_line(cur_line - 1, input, input_len);
                break;
            }

            case 'a':
            {
                char input[1024];
                int input_len = 0;
                char in_line[LINE_MAX];
                for (;;)
                {
                    read_stdin_line(in_line, LINE_MAX);
                    if (in_line[0] == '.' && in_line[1] == '\0')
                        break;
                    int l = strlen(in_line);
                    if (input_len + l + 1 > 1024)
                        break;
                    memcpy(input + input_len, in_line, l);
                    input_len += l;
                    input[input_len++] = '\n';
                }
                if (input_len > 0)
                    insert_line(cur_line, input, input_len);
                break;
            }

            case 'd':
                delete_line(cur_line);
                break;

            case 'p':
            {
                char num[16];
                for (int i = 0; i < num_lines; i++)
                {
                    int n = sprintf(num, "%d: ", i + 1);
                    write(1, num, n);
                    write(1, buf + line_offsets[i], line_lens[i]);
                    write(1, "\n", 1);
                }
                break;
            }

            case 'w':
                if (write_file(argv[1]) < 0)
                    printf("ed: write error\n");
                else
                    printf("wrote %ld bytes\n", buf_len);
                break;

            case 'q':
                return 0;

            default:
                if (cur_line >= 0 && cur_line < num_lines)
                    print_lines(cur_line, cur_line);
                break;
        }
    }
}
