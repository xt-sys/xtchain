/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/windres.c
 * DESCRIPTION: WINDRES compatible interface to LLVM
 * DEVELOPERS:  Josh de Kock <josh@itanimul.li>
 *              Martin Storsjo <martin@martin.st>
 *              Rafal Kupiec <belliash@codingworkshop.eu.org>
 */

#include "xtchain.h"

#define WINDRES_VERSION "1.0"

#ifndef DEFAULT_TARGET
#define DEFAULT_TARGET "x86_64-w64-mingw32"
#endif

#include <stdarg.h>

#define _tspawnvp_escape _spawnvp

#include <sys/wait.h>
#include <errno.h>

#define _P_WAIT 0

static
int
_spawnvp(int mode,
         const char *filename,
         const char * const *argv)
{
    pid_t pid;

    if(!(pid = fork()))
    {
        execvp(filename, (char **) argv);
        perror(filename);
        exit(1);
    }

    int stat = 0;

    if(waitpid(pid, &stat, 0) == -1)
    {
        return -1;
    }

    if(WIFEXITED(stat))
    {
        return WEXITSTATUS(stat);
    }
    errno = EIO;

    return -1;
}

static
const
char *unescape_cpp(const char *str)
{
    char *out = strdup(str);
    int len = strlen(str);
    int i, outpos = 0;

    for(i = 0; i < len - 1; i++)
    {
        if(str[i] == '\\' && str[i + 1] == '"')
        {
            continue;
        }
        out[outpos++] = str[i];
    }

    while(i < len)
    {
        out[outpos++] = str[i++];
    }

    out[outpos++] = '\0';

    return out;
}

static
void print_version(void)
{
    printf("XTchain windres (GNU windres compatible) %s\n", WINDRES_VERSION);
    exit(0);
}

static
void print_help(void)
{
    printf(
    "usage: llvm-windres <OPTION> [INPUT-FILE] [OUTPUT-FILE]\n"
    "\n"
    "LLVM Tool to manipulate Windows resources with a GNU windres interface.\n"
    "\n"
    "Options:\n"
    "  -i, --input <arg>          Name of the input file.\n"
    "  -o, --output <arg>         Name of the output file.\n"
    "  -J, --input-format <arg>   Input format to read.\n"
    "  -O, --output-format <arg>  Output format to generate.\n"
    "  --preprocessor <arg>       Custom preprocessor command.\n"
    "  --preprocessor-arg <arg>   Preprocessor command arguments.\n"
    "  -F, --target <arg>         Target for COFF objects to be compiled for.\n"
    "  -I, --include-dir <arg>    Include directory to pass to preprocessor and resource compiler.\n"
    "  -D, --define <arg[=val]>   Define to pass to preprocessor.\n"
    "  -U, --undefine <arg[=val]> Undefine to pass to preprocessor.\n"
    "  -c, --codepage <arg>       Default codepage to use when reading an rc file (0x0-0xffff).\n"
    "  -l, --language <arg>       Specify default language (0x0-0xffff).\n"
    "      --use-temp-file        Use a temporary file for the preprocessing output.\n"
    "  -v, --verbose              Enable verbose output.\n"
    "  -V, --version              Display version.\n"
    "  -h, --help                 Display this message and exit.\n"
    "Input Formats:\n"
    "  rc                         Text Windows Resource\n"
    "  res                        Binary Windows Resource\n"
    "Output Formats:\n"
    "  res                        Binary Windows Resource\n"
    "  coff                       COFF object\n"
    "Targets:\n"
    "  pe-x86-64\n"
    "  pei-x86-64\n"
    "  pe-i386\n"
    "  pei-i386\n");
    exit(0);
}

static
void error(const char *basename,
           const char *fmt,
           ...)
{
    fprintf(stderr, _T(TS": error: "), basename);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, _T("\n"));
    va_end(ap);
    exit(1);
}

static
void print_argv(const char **exec_argv)
{
    while(*exec_argv)
    {
        fprintf(stderr, _T(TS" "), *exec_argv);
        exec_argv++;
    }

    fprintf(stderr, _T("\n"));
}

static
void check_num_args(int arg,
                    int max_arg)
{
    if(arg > max_arg)
    {
        fprintf(stderr, "Too many options added\n");
        abort();
    }
}

int main(int argc,
         char* argv[])
{
    char *dir;
    char *basename;
    char *target;

    split_argv(argv[0], &dir, &basename, &target, NULL);

    if (!target)
        target = _T(DEFAULT_TARGET);

    const char *bfd_target = NULL;
    const char *input = _T("-");
    const char *output = _T("/dev/stdout");
    const char *input_format = _T("rc");
    const char *output_format = _T("coff");
    const char **includes = malloc(argc * sizeof(*includes));
    int nb_includes = 0;
    const char *codepage = _T("1252");
    const char *language = NULL;
    const char **cpp_options = malloc(argc * sizeof(*cpp_options));
    int nb_cpp_options = 0;
    int verbose = 0;

#define _tcslen_const(a) (sizeof(a)/sizeof(char) - 1)

#define _tcsstart(a, b) !strncmp(a, b, _tcslen_const(b))

#define IF_MATCH_EITHER(short, long) \
    if(!strcmp(argv[i], _T(short)) || !strcmp(argv[i], _T(long)))

#define IF_MATCH_THREE(first, second, third) \
    if(!strcmp(argv[i], _T(first)) || !strcmp(argv[i], _T(second)) || !strcmp(argv[i], _T(third)))

#define OPTION(short, long, var) \
    if(_tcsstart(argv[i], _T(short)) && argv[i][_tcslen_const(_T(short))]) { \
        var = argv[i] + _tcslen_const(_T(short)); \
    } else if(_tcsstart(argv[i], _T(long "="))) { \
        var = strchr(argv[i], '=') + 1; \
    } else IF_MATCH_EITHER(short, long) { \
        if(i + 1 < argc) \
            var = argv[++i]; \
        else \
            error(basename, _T(TS" missing argument"), argv[i]); \
    }

#define SEPARATE_ARG(var) do { \
        if(i + 1 < argc) \
            var = argv[++i]; \
        else \
            error(basename, _T(TS" missing argument"), argv[i]); \
    } while (0)

#define SEPARATE_ARG_PREFIX(var, prefix) do { \
        if(i + 1 < argc) \
            var = concat(_T(prefix), argv[++i]); \
        else \
            error(basename, _T(TS" missing argument"), argv[i]); \
    } while (0)

    for(int i = 1; i < argc; i++)
    {
        OPTION("-i", "--input", input)
        else OPTION("-o", "--output", output)
        else OPTION("-J", "--input-format", input_format)
        else OPTION("-O", "--output-format", output_format)
        else OPTION("-F", "--target", bfd_target)
        else IF_MATCH_THREE("-I", "--include-dir", "--include") {
            SEPARATE_ARG(includes[nb_includes++]);
        }
        else if(_tcsstart(argv[i], _T("--include-dir=")) ||
                _tcsstart(argv[i], _T("--include=")))
        {
            includes[nb_includes++] = strchr(argv[i], '=') + 1;
        }
        else if(_tcsstart(argv[i], _T("-I")))
        {
            includes[nb_includes++] = argv[i] + 2;
        }
        else OPTION("-c", "--codepage", codepage)
        else OPTION("-l", "--language", language)
        else if(!strcmp(argv[i], _T("--preprocessor")))
        {
            error(basename, _T("ENOSYS"));
        }
        else if(_tcsstart(argv[i], _T("--preprocessor-arg=")))
        {
            cpp_options[nb_cpp_options++] = strchr(argv[i], '=') + 1;
        }
        else if(!strcmp(argv[i], _T("--preprocessor-arg")))
        {
            SEPARATE_ARG(cpp_options[nb_cpp_options++]);
        }
        else IF_MATCH_EITHER("-D", "--define")
        {
            SEPARATE_ARG_PREFIX(cpp_options[nb_cpp_options++], "-D");
        }
        else if(_tcsstart(argv[i], _T("-D")))
        {
            cpp_options[nb_cpp_options++] = argv[i];
        }
        else IF_MATCH_EITHER("-U", "--undefine")
        {
            SEPARATE_ARG_PREFIX(cpp_options[nb_cpp_options++], "-U");
        }
        else if(_tcsstart(argv[i], _T("-U")))
        {
            cpp_options[nb_cpp_options++] = argv[i];
        }
        else IF_MATCH_EITHER("-v", "--verbose")
        {
            verbose = 1;
        }
        else IF_MATCH_EITHER("-V", "--version")
        {
            print_version();
        }
        else IF_MATCH_EITHER("-h", "--help")
        {
            print_help();
        }
        else if(!strcmp(argv[i], _T("--use-temp-file")))
        {
            // No-op, we use a temp file by default.
        }
        else if(_tcsstart(argv[i], _T("-")))
        {
            error(basename, _T("unrecognized option: `"TS"'"), argv[i]);
        }
        else
        {
            if(!strcmp(input, _T("-")))
            {
                input = argv[i];
            }
            else if(!strcmp(output, _T("/dev/stdout")))
            {
                output = argv[i];
            }
            else
            {
                error(basename, _T("rip: `"TS"'"), argv[i]);
            }
        }
    }

    if(bfd_target)
    {
        if(!strcmp(bfd_target, _T("pe-x86-64")) ||
           !strcmp(bfd_target, _T("pei-x86-64")))
        {
           target = _T("x86_64-w64-mingw32");
        }
        else if(!strcmp(bfd_target, _T("pe-i386")) ||
                !strcmp(bfd_target, _T("pei-i386")))
        {
            target = _T("i686-w64-mingw32");
        }
        else
        {
            error(basename, _T("unsupported target: `"TS"'"), bfd_target);
        }
    }

    char *arch = strdup(target);
    char *dash = strchr(arch, '-');
    if(dash)
    {
        *dash = '\0';
    }

    const char *machine = _T("unknown");

    if(!strcmp(arch, _T("i686")))
    {
        machine = _T("X86");
    }
    else if(!strcmp(arch, _T("x86_64")))
    {
        machine = _T("X64");
    }
    else if(!strcmp(arch, _T("armv7")))
    {
        machine = _T("ARM");
    }
    else if(!strcmp(arch, _T("aarch64")))
    {
        machine = _T("ARM64");
    }

    const char *CC = concat(target, _T("-clang"));
    const char **rc_options = malloc(2 * argc * sizeof(*cpp_options));
    int nb_rc_options = 0;

    for(int i = 0; i < nb_includes; i++)
    {
        cpp_options[nb_cpp_options++] = concat(_T("-I"), includes[i]);
        rc_options[nb_rc_options++] = _T("-I");
        rc_options[nb_rc_options++] = includes[i];
    }

    for(int i = 0; i < nb_cpp_options; i++)
    {
        cpp_options[i] = unescape_cpp(cpp_options[i]);
    }

    const char *preproc_rc = concat(output, _T(".preproc.rc"));
    const char *res = concat(output, _T(".out.res"));

    char *inputdir = strdup(input);
    {
        char *sep = _tcsrchrs(inputdir, '/', '\\');
        if(sep)
        {
            *sep = '\0';
        }
        else
        {
            inputdir = strdup(_T("."));
        }
    }

    int max_arg = 2 * argc + 20;
    const char **exec_argv = malloc((max_arg + 1) * sizeof(*exec_argv));
    int arg = 0;

    if(!_tcsicmp(input_format, _T("rc")))
    {
        exec_argv[arg++] = concat(dir, CC);
        exec_argv[arg++] = _T("-E");

        for(int i = 0; i < nb_cpp_options; i++)
        {
            exec_argv[arg++] = cpp_options[i];
        }

        exec_argv[arg++] = _T("-xc");
        exec_argv[arg++] = _T("-DRC_INVOKED=1");
        exec_argv[arg++] = input;
        exec_argv[arg++] = _T("-o");
        exec_argv[arg++] = preproc_rc;
        exec_argv[arg] = NULL;

        check_num_args(arg, max_arg);

        if(verbose)
        {
            print_argv(exec_argv);
        }

        int ret = _tspawnvp_escape(_P_WAIT, exec_argv[0], exec_argv);

        if(ret == -1)
        {
            perror(exec_argv[0]);
            return 1;
        }

        if(ret != 0)
        {
            error(basename, _T("preprocessor failed"));
            return ret;
        }

        arg = 0;
        exec_argv[arg++] = concat(dir, _T("llvm-rc"));

        for(int i = 0; i < nb_rc_options; i++)
        {
            exec_argv[arg++] = rc_options[i];
        }

        exec_argv[arg++] = _T("-I");
        exec_argv[arg++] = inputdir;
        exec_argv[arg++] = preproc_rc;
        exec_argv[arg++] = _T("-c");
        exec_argv[arg++] = codepage;

        if(language)
        {
            exec_argv[arg++] = _T("-l");
            exec_argv[arg++] = language;
        }

        exec_argv[arg++] = _T("-fo");

        if(!_tcsicmp(output_format, _T("res")))
        {
            exec_argv[arg++] = output;
        }
        else
        {
            exec_argv[arg++] = res;
        }

        exec_argv[arg] = NULL;
        check_num_args(arg, max_arg);

        if(verbose)
        {
            print_argv(exec_argv);
        }

        ret = _tspawnvp_escape(_P_WAIT, exec_argv[0], exec_argv);

        if(ret == -1)
        {
            perror(exec_argv[0]);
            return 1;
        }

        if(ret != 0)
        {
            error(basename, _T("llvm-rc failed"));
            if(!verbose)
            {
                unlink(preproc_rc);
            }
            return ret;
        }

        if(!_tcsicmp(output_format, _T("res")))
        {
            // All done
        }
        else if(!_tcsicmp(output_format, _T("coff")))
        {
            arg = 0;
            exec_argv[arg++] = concat(dir, _T("llvm-cvtres"));
            exec_argv[arg++] = res;
            exec_argv[arg++] = concat(_T("-machine:"), machine);
            exec_argv[arg++] = concat(_T("-out:"), output);
            exec_argv[arg] = NULL;

            check_num_args(arg, max_arg);
            if(verbose)
            {
                print_argv(exec_argv);
            }

            int ret = _tspawnvp_escape(_P_WAIT, exec_argv[0], exec_argv);
            if(ret == -1)
            {
                perror(exec_argv[0]);
                return 1;
            }

            if(!verbose)
            {
                unlink(preproc_rc);
                unlink(res);
            }

            return ret;
        } else {
            error(basename, _T("invalid output format: `"TS"'"), output_format);
        }
    }
    else if(!_tcsicmp(input_format, _T("res")))
    {
        exec_argv[arg++] = concat(dir, _T("llvm-cvtres"));
        exec_argv[arg++] = input;
        exec_argv[arg++] = concat(_T("-machine:"), machine);
        exec_argv[arg++] = concat(_T("-out:"), output);
        exec_argv[arg] = NULL;

        check_num_args(arg, max_arg);

        if(verbose)
        {
            print_argv(exec_argv);
        }

        int ret = _tspawnvp_escape(_P_WAIT, exec_argv[0], exec_argv);
        if(ret == -1)
        {
            perror(exec_argv[0]);
            return 1;
        }

        return ret;
    }
    else
    {
        error(basename, _T("invalid input format: `"TS"'"), input_format);
    }

    return 0;
}
