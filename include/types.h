#ifndef OSB_TYPES_H
#define OSB_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#define OSB_OK        0
#define OSB_ERR      -1
#define OSB_ENOMEM   -2
#define OSB_EIO      -3
#define OSB_EDISK    -4
#define OSB_EBADARG  -5
#define OSB_ENOENT   -6   /* no such file / entry / module   */
#define OSB_EACCES   -7   /* authentication failed           */
#define OSB_ELOCKED  -8   /* account or session is locked    */
#define OSB_EBUSY    -9   /* refcount > 0, cannot unload     */
#define OSB_EBADFMT -10   /* malformed config / disk image   */

#define OSB_MAX(a, b) ((a) > (b) ? (a) : (b))
#define OSB_MIN(a, b) ((a) < (b) ? (a) : (b))

/* Two-way string equality that treats NULL as the empty string, so config
 * and seed parsing never has to null-check every value first. */
static inline int osb_streq(const char *a, const char *b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

#endif /* OSB_TYPES_H */
