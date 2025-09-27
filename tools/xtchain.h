/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/xtchain.h
 * DESCRIPTION: Common header for XTchain tools
 * DEVELOPERS:  Martin Storsjo <martin@martin.st>
 *              Rafal Kupiec <belliash@codingworkshop.eu.org>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#define SECTOR_SIZE     512
#define _T(x)           x

typedef struct MBR_PARTITION {
    uint8_t BootFlag;       // 0x80 = bootable, 0x00 = non-boot
    uint8_t StartCHS [3];   // CHS address
    uint8_t Type;           // Partition type
    uint8_t EndCHS[3];      // CHS address
    uint32_t StartLBA;      // Start sector
    uint32_t Size;          // Sectors count
} MBR_PARTITION, *PMBR_PARTITION;

static
inline
char *
_tcsrchrs(const char *str,
          char char1,
          char char2)
{
    char *ptr1 = strrchr(str, char1);
    char *ptr2 = strrchr(str, char2);

    if(!ptr1)
    {
        return ptr2;
    }

    if(!ptr2)
    {
        return ptr1;
    }

    if(ptr1 < ptr2)
    {
        return ptr2;
    }

    return ptr1;
}

static
inline
void
split_argv(const char *argv0,
           char **dir_ptr,
           char **basename_ptr,
           char **target_ptr,
           char **exe_ptr)
{
    const char *sep = _tcsrchrs(argv0, '/', '\\');
    const char *basename_ptr_const = argv0;
    char *dir = strdup(_T(""));

    if(sep)
    {
        dir = strdup(argv0);
        dir[sep + 1 - argv0] = '\0';
        basename_ptr_const = sep + 1;
    }

    char *basename = strdup(basename_ptr_const);
    char *period = strchr(basename, '.');

    if(period)
    {
        *period = '\0';
    }

    char *target = strdup(basename);
    char *dash = strrchr(target, '-');
    char *exe = basename;

    if(dash)
    {
        *dash = '\0';
        exe = dash + 1;
    }
    else
    {
        target = NULL;
    }

    if(dir_ptr)
    {
        *dir_ptr = dir;
    }

    if(basename_ptr)
    {
        *basename_ptr = basename;
    }

    if(target_ptr)
    {
        *target_ptr = target;
    }

    if(exe_ptr)
    {
        *exe_ptr = exe;
    }
}
