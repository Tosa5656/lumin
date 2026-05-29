#ifndef _STRING_H
#define _STRING_H

void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
int   memcmp(const void *a, const void *b, unsigned long n);
void *memchr(const void *s, int c, unsigned long n);

unsigned long strlen(const char *s);
int   strcmp(const char *a, const char *b);
int   strncmp(const char *a, const char *b, unsigned long n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, unsigned long n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);

int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, unsigned long n);
char *strpbrk(const char *s, const char *accept);
unsigned long strspn(const char *s, const char *accept);
unsigned long strcspn(const char *s, const char *reject);
char *strncat(char *dst, const char *src, unsigned long n);
char *strerror(int errnum);

#endif
