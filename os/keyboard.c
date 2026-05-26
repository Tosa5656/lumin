#include "keyboard.h"
#include "ports.h"
#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"

extern unsigned char keyboard_color;

static int shift_pressed;
static int ctrl_pressed;
static int caps_locked;
static int extended;

static volatile char keybuf[KEYBUF_SIZE];
static volatile int keybuf_head;
static volatile int keybuf_tail;

static const char keymap[128] = {
    [0x01] = 0,    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',
    [0x05] = '4',  [0x06] = '5',  [0x07] = '6',  [0x08] = '7',
    [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0',  [0x0C] = '-',
    [0x0D] = '=',  [0x0E] = 0,    [0x0F] = 0,
    [0x10] = 'q',  [0x11] = 'w',  [0x12] = 'e',  [0x13] = 'r',
    [0x14] = 't',  [0x15] = 'y',  [0x16] = 'u',  [0x17] = 'i',
    [0x18] = 'o',  [0x19] = 'p',  [0x1A] = '[',  [0x1B] = ']',
    [0x1C] = '\n', [0x1D] = 0,
    [0x1E] = 'a',  [0x1F] = 's',  [0x20] = 'd',  [0x21] = 'f',
    [0x22] = 'g',  [0x23] = 'h',  [0x24] = 'j',  [0x25] = 'k',
    [0x26] = 'l',  [0x27] = ';',  [0x28] = '\'', [0x29] = '`',
    [0x2A] = 0,    [0x2B] = '\\',
    [0x2C] = 'z',  [0x2D] = 'x',  [0x2E] = 'c',  [0x2F] = 'v',
    [0x30] = 'b',  [0x31] = 'n',  [0x32] = 'm',  [0x33] = ',',
    [0x34] = '.',  [0x35] = '/',  [0x36] = 0,    [0x37] = '*',
    [0x38] = 0,    [0x39] = ' '
};

static const char keymap_shift[128] = {
    [0x02] = '!',  [0x03] = '@',  [0x04] = '#',  [0x05] = '$',
    [0x06] = '%',  [0x07] = '^',  [0x08] = '&',  [0x09] = '*',
    [0x0A] = '(',  [0x0B] = ')',  [0x0C] = '_',  [0x0D] = '+',
    [0x10] = 'Q',  [0x11] = 'W',  [0x12] = 'E',  [0x13] = 'R',
    [0x14] = 'T',  [0x15] = 'Y',  [0x16] = 'U',  [0x17] = 'I',
    [0x18] = 'O',  [0x19] = 'P',  [0x1A] = '{',  [0x1B] = '}',
    [0x1E] = 'A',  [0x1F] = 'S',  [0x20] = 'D',  [0x21] = 'F',
    [0x22] = 'G',  [0x23] = 'H',  [0x24] = 'J',  [0x25] = 'K',
    [0x26] = 'L',  [0x27] = ':',  [0x28] = '"',  [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z',  [0x2D] = 'X',  [0x2E] = 'C',  [0x2F] = 'V',
    [0x30] = 'B',  [0x31] = 'N',  [0x32] = 'M',  [0x33] = '<',
    [0x34] = '>',  [0x35] = '?',
};

void keyboard_init(void)
{
    keybuf_head = 0;
    keybuf_tail = 0;
}

void keyboard_handler(void)
{
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6)
    {
        shift_pressed = 0;
        return;
    }
    if (scancode == 0x1D)
    {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x9D)
    {
        ctrl_pressed = 0;
        return;
    }
    if (scancode == 0x3A)
    {
        caps_locked = !caps_locked;
        return;
    }

    if (scancode == 0xE0)
    {
        extended = 1;
        return;
    }
    if (extended)
    {
        extended = 0;
        if (scancode & 0x80) return;
        char special = 0;
        if (scancode == 0x48) special = KEY_UP;
        else if (scancode == 0x50) special = KEY_DOWN;
        else if (scancode == 0x4B) special = KEY_LEFT;
        else if (scancode == 0x4D) special = KEY_RIGHT;
        else if (scancode == 0x47) special = KEY_HOME;
        else if (scancode == 0x4F) special = KEY_END;
        else if (scancode == 0x53) special = KEY_DEL;
        if (!special) return;
        int next = (keybuf_head + 1) & (KEYBUF_SIZE - 1);
        if (next != keybuf_tail)
        {
            keybuf[keybuf_head] = special;
            keybuf_head = next;
        }
        return;
    }

    if (scancode & 0x80)
        return;

    int upper = shift_pressed;
    if (caps_locked && scancode >= 0x10 && scancode <= 0x32)
    {
        if ((scancode >= 0x10 && scancode <= 0x19) ||
            (scancode >= 0x1E && scancode <= 0x26) ||
            (scancode >= 0x2C && scancode <= 0x32))
            upper = !upper;
    }

    char c;
    if (upper)
        c = keymap_shift[scancode];
    else
        c = keymap[scancode];

    if (ctrl_pressed && c >= 'a' && c <= 'z')
        c &= 0x1F;
    if (ctrl_pressed && c >= 'A' && c <= 'Z')
        c &= 0x1F;

    if (scancode == 0x0E)
        c = '\b';
    else if (!c)
        return;

    int next = (keybuf_head + 1) & (KEYBUF_SIZE - 1);
    if (next != keybuf_tail)
    {
        keybuf[keybuf_head] = (scancode == 0x0E) ? '\b' : c;
        keybuf_head = next;
    }
}

static int hist_strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static tab_complete_fn tab_complete_hook;

void keyboard_set_tab_complete(tab_complete_fn fn)
{
    tab_complete_hook = fn;
}

static void vga_redraw_line(const char *buf, int idx, int cursor,
                            int start_row, int start_col)
{
    volatile unsigned short *vga_buf = (volatile unsigned short *)VGA_ADDRESS;
    unsigned char color = keyboard_color;


    int r = start_row;
    int c = start_col;

    for (int i = 0; i < idx; i++)
    {
        vga_buf[r * VGA_WIDTH + c] = vga_entry((unsigned char)buf[i], color);
        if (++c == VGA_WIDTH) { c = 0; r++; }
    }

    while (r * VGA_WIDTH + c < (start_row * VGA_WIDTH + start_col + idx + 256)
           && r < VGA_HEIGHT)
    {
        vga_buf[r * VGA_WIDTH + c] = vga_entry(' ', color);
        if (++c == VGA_WIDTH) { c = 0; r++; }
        if (r * VGA_WIDTH + c >= start_row * VGA_WIDTH + start_col + VGA_WIDTH * 2)
            break;
    }

    {
        int cr = start_row + (start_col + cursor) / VGA_WIDTH;
        int cc = (start_col + cursor) % VGA_WIDTH;
        if (cr >= VGA_HEIGHT) { cr = VGA_HEIGHT - 1; cc = VGA_WIDTH - 1; }
        vga_set_cursor(cr, cc);
    }
}

static void serial_echo_line(const char *buf, int idx)
{
    for (int i = 0; i < idx; i++)
        serial_putchar(buf[i]);
}

int keyboard_readline(char *buf, int max)
{
    static char history[HISTORY_MAX][256];
    static int hist_count = 0;

    char saved[256];
    int saved_len = 0;
    int browsing = -1;
    int idx = 0;
    int cursor = 0;
    int start_row, start_col;

    vga_get_cursor(&start_row, &start_col);

    while (1)
    {
        int ser = serial_readchar();
        while (ser >= 0)
        {
            int next = (keybuf_head + 1) & (KEYBUF_SIZE - 1);
            if (next != keybuf_tail)
            {
                keybuf[keybuf_head] = ser;
                keybuf_head = next;
            }
            ser = serial_readchar();
        }

        if (keybuf_tail == keybuf_head)
            __asm__("sti; hlt");

        while (keybuf_tail != keybuf_head)
        {
            unsigned char c = (unsigned char)keybuf[keybuf_tail];
            keybuf_tail = (keybuf_tail + 1) & (KEYBUF_SIZE - 1);

            if (c == '\n')
            {
                buf[idx] = '\0';
                vga_putchar('\n', keyboard_color);
                serial_putchar('\n');

                if (idx > 0 && (hist_count == 0 ||
                    hist_strcmp(history[hist_count - 1], buf) != 0))
                {
                    if (hist_count == HISTORY_MAX)
                    {
                        for (int i = 1; i < HISTORY_MAX; i++)
                            for (int j = 0; j < 256; j++)
                                history[i - 1][j] = history[i][j];
                        hist_count--;
                    }
                    int i;
                    for (i = 0; i < idx && i < 256 - 1; i++)
                        history[hist_count][i] = buf[i];
                    history[hist_count][i] = '\0';
                    hist_count++;
                }
                return idx;
            }

            if (c == '\b')
            {
                if (cursor > 0)
                {
                    int shift = idx - cursor;
                    for (int i = 0; i < shift; i++)
                        buf[cursor - 1 + i] = buf[cursor + i];
                    idx--;
                    cursor--;
                    vga_redraw_line(buf, idx, cursor, start_row, start_col);
                    serial_write("\b \b");
                }
                continue;
            }

            if (c == KEY_DEL)
            {
                if (cursor < idx)
                {
                    int shift = idx - cursor - 1;
                    for (int i = 0; i < shift; i++)
                        buf[cursor + i] = buf[cursor + 1 + i];
                    idx--;
                    vga_redraw_line(buf, idx, cursor, start_row, start_col);
                }
                continue;
            }

            if (c == 0x15)
            {
                if (idx > 0)
                {
                    idx = 0;
                    cursor = 0;
                    vga_redraw_line(buf, idx, cursor, start_row, start_col);
                    serial_write("\b \b");
                }
                continue;
            }

            if (c == KEY_LEFT)
            {
                if (cursor > 0)
                {
                    cursor--;
                    int r = start_row + (start_col + cursor) / VGA_WIDTH;
                    int col = (start_col + cursor) % VGA_WIDTH;
                    vga_set_cursor(r, col);
                }
                continue;
            }

            if (c == KEY_RIGHT)
            {
                if (cursor < idx)
                {
                    cursor++;
                    int r = start_row + (start_col + cursor) / VGA_WIDTH;
                    int col = (start_col + cursor) % VGA_WIDTH;
                    vga_set_cursor(r, col);
                }
                continue;
            }

            if (c == KEY_HOME)
            {
                cursor = 0;
                vga_set_cursor(start_row, start_col);
                continue;
            }

            if (c == KEY_END)
            {
                cursor = idx;
                {
                    int r = start_row + (start_col + cursor) / VGA_WIDTH;
                    int col = (start_col + cursor) % VGA_WIDTH;
                    vga_set_cursor(r, col);
                }
                continue;
            }

            if (c == '\t')
            {
                if (tab_complete_hook)
                {
                    int result = tab_complete_hook(buf, &cursor, idx, max);
                    if (result > 0)
                    {
                        idx = cursor;
                        vga_redraw_line(buf, idx, cursor, start_row, start_col);
                    }
                }
                continue;
            }

            if (c == KEY_UP)
            {
                if (hist_count == 0) continue;

                if (browsing == -1)
                {
                    for (int i = 0; i < idx && i < 255; i++) saved[i] = buf[i];
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
                vga_redraw_line(buf, idx, cursor, start_row, start_col);
                serial_write("\r");
                serial_echo_line(buf, idx);
                continue;
            }

            if (c == KEY_DOWN)
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
                vga_redraw_line(buf, idx, cursor, start_row, start_col);
                serial_write("\r");
                serial_echo_line(buf, idx);
                continue;
            }

            if (idx < max - 1)
            {
                for (int i = idx; i > cursor; i--)
                    buf[i] = buf[i - 1];
                buf[cursor] = c;
                idx++;
                cursor++;
                vga_redraw_line(buf, idx, cursor, start_row, start_col);
                serial_putchar(c);
            }
        }
    }
}
