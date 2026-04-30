#include <stddef.h>

void *__attribute__((weak)) lv_utils_bsearch(const void *key, const void *base, size_t n, size_t size,
                                             int (*cmp)(const void *pRef, const void *pElement))
{
    const unsigned char *items = (const unsigned char *)base;
    size_t left = 0;
    size_t right = n;

    while(left < right) {
        size_t mid = left + (right - left) / 2;
        const void *candidate = items + (mid * size);
        int result = cmp(key, candidate);

        if(result < 0) {
            right = mid;
        }
        else if(result > 0) {
            left = mid + 1;
        }
        else {
            return (void *)candidate;
        }
    }

    return NULL;
}
