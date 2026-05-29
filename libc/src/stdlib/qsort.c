#include <stdlib.h>
#include <string.h>

static void swap(char *a, char *b, unsigned long size)
{
    for (unsigned long i = 0; i < size; i++)
    {
        char t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

void qsort(void *base, unsigned long nmemb, unsigned long size,
           int (*compar)(const void *, const void *))
{
    if (nmemb <= 1) return;

    char *arr = (char *)base;
    unsigned long last = nmemb - 1;
    unsigned long pivot = 0;

    for (unsigned long i = 0; i < last; i++)
    {
        if (compar(&arr[i * size], &arr[last * size]) < 0)
        {
            swap(&arr[pivot * size], &arr[i * size], size);
            pivot++;
        }
    }

    swap(&arr[pivot * size], &arr[last * size], size);

    if (pivot > 1)
        qsort(arr, pivot, size, compar);

    if (last - pivot > 1)
        qsort(&arr[(pivot + 1) * size], last - pivot, size, compar);
}
