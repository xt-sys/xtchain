/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/xtadkgen.c
 * DESCRIPTION: XT Assembly Development Kit generator
 * DEVELOPERS:  Aiken Harris <harraiken91@gmail.com>
 */

#include "xtchain.h"


#define MAX_LINE_LEN 1024
#define MAGIC_MARKER "==> "

/* Prints usage */
void PrintUsage(const char* ProgramName)
{
    /* Output usage instructions to stderr */
    fprintf(stderr, "Usage: %s <input1.S> [input2.S ...] -o <output.h>\n", ProgramName);
}

/* Processes single .S file */
void ProcessSourceFile(FILE* InFile, FILE* OutFile)
{
    char LineBuffer[MAX_LINE_LEN];
    char SymbolName[256];
    char ValueString[256];
    char *ValuePointer;
    char *Marker;

    /* Iterate through the input stream until the end of the file */
    while(fgets(LineBuffer, sizeof(LineBuffer), InFile))
    {
        Marker = strstr(LineBuffer, MAGIC_MARKER);

        /* Search for the predefined translation marker */
        if(Marker)
        {
            /* Extract the symbol identifier and its associated value */
            if(sscanf(Marker, "==> %255s %255s", SymbolName, ValueString) == 2)
            {
                /* Initialize pointer for character-level cleanup */
                ValuePointer = ValueString;

                /* Strip architecture-specific immediate prefixes such as '$' (AT&T) or '#' (ARM) */
                while(*ValuePointer == '$' || *ValuePointer == '#')
                {
                    /* Advance pointer to the first alpha-numeric character */
                    ValuePointer++;
                }

                /* Generate the corresponding C-preprocessor macro definition */
                fprintf(OutFile, "#undef %s\n", SymbolName);
                fprintf(OutFile, "#define %s %s\n\n", SymbolName, ValuePointer);
            }
        }
    }
}

/* Main function */
int main(int Argc, char* Argv[])
{
    FILE* InFile;
    FILE* OutFile = stdout;
    const char* OutputName = NULL;
    int FirstInputIndex = -1;
    int InputCount = 0;

    /* Validate that a minimum set of command-line arguments is present */
    if(Argc < 2)
    {
        /* Display usage information and terminate on insufficient arguments */
        PrintUsage(Argv[0]);
        return 1;
    }

    /* Parse command-line arguments */
    for(int Index = 1; Index < Argc; Index++)
    {
        /* Identify the output file switch */
        if((strcmp(Argv[Index], "-o") == 0 || strcmp(Argv[Index], "-O") == 0) && Index + 1 < Argc)
        {
            /* Assign the designated output path and skip the next argument */
            OutputName = Argv[++Index];
        }
        else
        {
            /* Track the first occurrence of an input file for the processing loop */
            if(FirstInputIndex == -1)
            {
                /* Store the argument index for the subsequent file operations */
                FirstInputIndex = Index;
            }

            /* Increment the total count of input files */
            InputCount++;
        }
    }

    /* Ensure at least one input source is available for processing */
    if(InputCount == 0 || (OutputName == NULL && FirstInputIndex == -1))
    {
        /* Print usage and exit */
        PrintUsage(Argv[0]);
        return 1;
    }

    /* Initialize the output stream either to a file or default to stdout */
    if(OutputName)
    {
        /* Attempt to open the specified output header for write access */
        OutFile = fopen(OutputName, "w");
        if(!OutFile)
        {
            /* Emit fatal error if the destination file cannot be accessed */
            fprintf(stderr, "%s: Failed to open output file '%s'\n", Argv[0], OutputName);
            return 1;
        }
    }

    /* Write the auto-generation warning and inclusion guard */
    fprintf(OutFile, "// FILE GENERATED AUTOMATICALLY BY '%s'. DO NOT EDIT!\n\n", Argv[0]);
    fprintf(OutFile, "#ifndef __XTADK_H_\n");
    fprintf(OutFile, "#define __XTADK_H_\n\n");

    /* Loop through the arguments again to process each input file */
    for(int Index = 1; Index < Argc; Index++)
    {
        /* Skip the output switch and its associated parameter */
        if(strcmp(Argv[Index], "-o") == 0 || strcmp(Argv[Index], "-O") == 0)
        {
            /* Advance the loop counter and continue  */
            Index++;
            continue;
        }

        /* Open the current assembly source file */
        InFile = fopen(Argv[Index], "r");
        if(!InFile)
        {
            /* Report error for the missing file and proceed to the next input file */
            fprintf(stderr, "%s: Failed to open file '%s', skipping.\n", Argv[0], Argv[Index]);
            continue;
        }

        /* Add source file data to the generated header */
        fprintf(OutFile, "/* Source: %s */\n", Argv[Index]);
        ProcessSourceFile(InFile, OutFile);
        fprintf(OutFile, "\n");

        /* Close the input file handle */
        fclose(InFile);
    }

    /* Close the inclusion guard in the output */
    fprintf(OutFile, "#endif /* __XTADK_H_ */\n");

    /* Check if a file handle was utilized */
    if(OutFile != stdout)
    {
        /* Commit buffered data and close the output file */
        fclose(OutFile);
    }

    /* Return success */
    return 0;
}
