#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEYBUF_SIZE 256

void keyboard_init(void);
int keyboard_readline(char *buf, int max);

#endif
