#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEYBUF_SIZE 256
#define KEY_UP      0x80
#define KEY_DOWN    0x81
#define KEY_LEFT    0x82
#define KEY_RIGHT   0x83
#define KEY_HOME    0x84
#define KEY_END     0x85
#define KEY_DEL     0x86
#define HISTORY_MAX 16

/* tab-completion hook: given the buffer, cursor position (in/out),
   current line length, and max size, modifies buf at *pos,
   advances *pos, returns number of chars inserted.
   On return, *pos equals the new line length. */
typedef int (*tab_complete_fn)(char *buf, int *pos, int len, int max);

void keyboard_init(void);
int keyboard_readline(char *buf, int max);
void keyboard_set_tab_complete(tab_complete_fn fn);

#endif
