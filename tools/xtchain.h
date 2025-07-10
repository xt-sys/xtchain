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
#include <stdlib.h>
#include <string.h>

#define _T(x) x

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
    char *dir = strdup(_T(""));
    const char *basename = argv0;

    if(sep)
    {
        dir = strdup(argv0);
        dir[sep + 1 - argv0] = '\0';
        basename = sep + 1;
    }

    basename = strdup(basename);
    char *period = strchr(basename, '.');

    if(period)
    {
        *period = '\0';
    }

    char *target = strdup(basename);
    char *dash = strrchr(target, '-');
    const char *exe = basename;

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
