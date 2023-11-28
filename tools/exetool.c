/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/exetool.c
 * DESCRIPTION: Portable Executable (PE) utility for changing subsystem
 * DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
 */

#include "xtchain.h"

typedef struct _PE_SUBSYSTEM
{
    int Identifier;
    char *Name;
} PE_SUBSYSTEM, *PPE_SUBSYSTEM;

static PE_SUBSYSTEM SubSystems[] = {
    {0x00, "INVALID_SUBSYSTEM"},
    {0x01, "NT_NATIVE"},
    {0x02, "WINDOWS_GUI"},
    {0x03, "WINDOWS_CLI"},
    {0x04, "WINDOWS_CE_OLD"},
    {0x05, "OS2_CUI"},
    {0x07, "POSIX_CUI"},
    {0x08, "NATIVE_WINDOWS"},
    {0x09, "WINDOWS_CE_GUI"},
    {0x0A, "EFI_APPLICATION"},
    {0x0B, "EFI_BOOT_SERVICE_DRIVER"},
    {0x0C, "EFI_RUNTIME_DRIVER"},
    {0x0D, "EFI_ROM"},
    {0x0E, "XBOX"},
    {0x10, "WINDOWS_BOOT_APPLICATION"},
    {0x14, "XT_NATIVE_KERNEL"},
    {0x15, "XT_NATIVE_APPLICATION"},
    {0x16, "XT_NATIVE_DRIVER"},
    {0x17, "XT_DYNAMIC_LIBRARY"},
    {0x18, "XT_APPLICATION_CLI"},
    {0x19, "XT_APPLICATION_GDI"}
};

PPE_SUBSYSTEM getSubSystem(char *Name)
{
    int Index;
    int SubSystemsCount;
    PPE_SUBSYSTEM SubSystem;

    /* Count number of subsystems avaialble */
    SubSystemsCount = sizeof(SubSystems) / sizeof(PE_SUBSYSTEM);

    /* Find subsystem */
    for(Index = 0; Index < SubSystemsCount; Index++)
    {
        SubSystem = &SubSystems[Index];
        if(strcasecmp(SubSystem->Name, Name) == 0)
        {
            /* Subsystem found, return its ID */
            return SubSystem;
        }
    }

	/* No valid subsystem found */
    return &SubSystems[0];
}

char *getSubSystemName(int Identifier)
{
    int Index;
    int SubSystemsCount;
    PPE_SUBSYSTEM SubSystem;

    /* Count number of subsystems avaialble */
    SubSystemsCount = sizeof(SubSystems) / sizeof(PE_SUBSYSTEM);

    /* Find subsystem */
    for(Index = 0; Index < SubSystemsCount; Index++)
    {
        SubSystem = &SubSystems[Index];
        if(SubSystem->Identifier == Identifier)
        {
            /* Subsystem found, return its ID */
            return SubSystem->Name;
        }
    }

	/* No valid subsystem found */
    return SubSystems[0].Name;
}

int main(int argc, char *argv[])
{
    FILE *ExeFile;
    unsigned char Signature[4];
    unsigned int HeaderOffset;
    unsigned short SubSystem;
    PPE_SUBSYSTEM NewSubSystem;

    /* Check for proper number of arguments */
    if(argc != 3)
    {
        printf("Usage: %s <filename> <new SubSystem>\n", argv[0]);
        return 1;
    }

    /* Open the EXE file in binary mode */
    ExeFile = fopen(argv[1], "r+b");
    if(ExeFile == NULL)
    {
        /* Failed to open PE file */
        printf("ERROR: Unable to open file %s\n", argv[1]);
        return 1;
    }
    
    /* Verify that the input file has a valid DOS header */
    fread(Signature, sizeof(unsigned char), 4, ExeFile);
    if(Signature[0] != 'M' || Signature[1] != 'Z')
    {
        /* Invalid DOS header */
        printf("ERROR: %s is not a valid EXE file\n", argv[1]);
        fclose(ExeFile);
        return 1;
    }

    /* Verify that the input file has a valid PE header */
    fseek(ExeFile, 0x3C, SEEK_SET);
    fread(&HeaderOffset, sizeof(unsigned int), 1, ExeFile);
    fseek(ExeFile, HeaderOffset, SEEK_SET);
    fread(Signature, sizeof(unsigned char), 4, ExeFile);
    if(Signature[0] != 'P' || Signature[1] != 'E' || Signature[2] != 0 || Signature[3] != 0)
    {
        /* Invalid PE header */
        printf("Error: %s is not a valid PE file\n", argv[1]);
        fclose(ExeFile);
        return 1;
    }

    /* Seek to the offset of the SubSystem field in the optional header */
    fseek(ExeFile, HeaderOffset + 0x5C, SEEK_SET);

    /* Read the current SubSystem value */
    fread(&SubSystem, sizeof(unsigned short), 1, ExeFile);

    /* Parse the new SubSystem value from the command line argument */
    NewSubSystem = getSubSystem(argv[2]);
    if(NewSubSystem->Identifier == 0)
    {
        /* Invalid SubSystem provided */
        printf("Error: %s is not a valid PE SubSystem\n", argv[2]);
        return 1;
    }

    /* Print new SubSystem identifier */

    /* Seek back to the SubSystem field in the optional header */
    fseek(ExeFile, -sizeof(unsigned short), SEEK_CUR);

    /* Write the new SubSystem value */
    fwrite(&NewSubSystem->Identifier, sizeof(unsigned short), 1, ExeFile);

    /* Close the file */
    fclose(ExeFile);

    /* Finished successfully */
    printf("PE SubSystem modified: 0x%04X <%s> to 0x%04X <%s>\n",
           SubSystem, getSubSystemName(SubSystem), NewSubSystem->Identifier, NewSubSystem->Name);
    return 0;
}
