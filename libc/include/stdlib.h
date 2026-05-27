#ifndef _STDLIB_H
#define _STDLIB_H

void exit(int code);
void abort(void);

void *malloc(unsigned long size);
void free(void *ptr);
void *calloc(unsigned long nmemb, unsigned long size);
void *realloc(void *ptr, unsigned long size);

int atoi(const char *s);

#endif