#ifndef _STDLIB_H
#define _STDLIB_H

void exit(int code);
void abort(void);

void *malloc(unsigned long size);
void free(void *ptr);
void *calloc(unsigned long nmemb, unsigned long size);
void *realloc(void *ptr, unsigned long size);

int atoi(const char *s);
long strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);

int rand(void);
void srand(unsigned int seed);

void qsort(void *base, unsigned long nmemb, unsigned long size,
           int (*compar)(const void *, const void *));

int atexit(void (*func)(void));
void _Exit(int code);

int abs(int n);
long labs(long n);

#endif
