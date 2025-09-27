/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/diskimg.c
 * DESCRIPTION: Disk Image manipulation tool
 * DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
 */

#include "xtchain.h"


/* Loads a sector from a file */
int LoadSector(const char *FileName, uint8_t *Buffer)
{
    FILE *File;
    long Size;

    /* Open the file in binary mode */
    File= fopen(FileName, "rb");
    if(!File)
    {
        /* Failed to open file */
        perror("Failed to open sector file");
        return -1;
    }

    /* Check the file size */
    fseek(File, 0, SEEK_END);
    Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    if(Size != SECTOR_SIZE)
    {
        /* File is not exactly 512 bytes */
        fprintf(stderr, "Error: file '%s' must be exactly 512 bytes.\n", FileName);
        fclose(File);
        return -1;
    }

    /* Read sector to buffer */
    if(fread(Buffer, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
    {
        /* Failed to read sector */
        perror("Failed to read sector from file");
        fclose(File);
        return -1;
    }

    /* Close the file */
    fclose(File);
    return 0;
}

/* Main function */
int main(int argc, char **argv)
{
    FILE *File;
    char FormatCommand[512];
    long FormatPartition = 0;
    long DiskSizeBytes = 0;
    long DiskSizeMB = 0;
    MBR_PARTITION Partition = {0};
    char Zero[SECTOR_SIZE] = {0};
    uint8_t Mbr[SECTOR_SIZE] = {0};
    uint8_t Vbr[SECTOR_SIZE];
    const char *FileName = NULL;
    const char *MbrFile = NULL;
    const char *VbrFile = NULL;

    /* Parse command line arguments */
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            /* Format partition */
            FormatPartition = 1;
        }
        else if(strcmp(argv[i], "-m") == 0 && i + 1 < argc)
        {
            /* MBR file */
            MbrFile = argv[++i];
        }
        else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            /* Output file */
            FileName = argv[++i];
        }
        else if(strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            /* Disk size */
            DiskSizeMB = atol(argv[++i]);
        }
        else if(strcmp(argv[i], "-v") == 0 && i + 1 < argc)
        {
            /* VBR file */
            VbrFile = argv[++i];
        }
        else
        {
            /* Unknown argument */
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    /* Check for required arguments */
    if(DiskSizeMB <= 0 || FileName == NULL)
    {
        /* Missing required arguments, print usage */
        fprintf(stderr, "Usage: %s -s <size_MB> -o <output.img> [-m <mbr.img>] [-v <vbr.img>]\n", argv[0]);
        return 1;
    }

    /* Calculate disk size in bytes */
    DiskSizeBytes = DiskSizeMB * 1024 * 1024;

    /* Open the output file in binary mode */
    File = fopen(FileName, "wb");
    if(!File) {
        /* Failed to open file */
        perror("Failed to open disk image file");
        return 1;
    }

    /* Write zeros to the disk image file */
    for(long i = 0; i < DiskSizeBytes / SECTOR_SIZE; i++)
    {
        if(fwrite(Zero, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to write to disk image file */
            perror("Failed to write to disk image file");
            fclose(File);
            return 1;
        }
    }

    /* Load MBR if provided */
    if(MbrFile)
    {
        if(LoadSector(MbrFile, Mbr) != 0)
        {
            /* Failed to load MBR from file */
            perror("Failed to load MBR from file");
            fclose(File);
            return 1;
        }
    }

    /* Setup MBR partition as W95 FAT32 */
    Partition.BootFlag = 0x80;
    Partition.Type = 0x0B;
    Partition.StartLBA = 2048;
    Partition.Size = (DiskSizeBytes / SECTOR_SIZE) - 2048;

    /* Write MBR */
    memcpy(&Mbr[446], &Partition, sizeof(MBR_PARTITION));
    Mbr[510] = 0x55;
    Mbr[511] = 0xAA;

    /* Write the MBR to the beginning of the disk image */
    fseek(File, 0, SEEK_SET);
    if(fwrite(Mbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
    {
        perror("Failed to write MBR to disk image");
        fclose(File);
        return 1;
    }

    /* Check if we need to format the partition */
    if(FormatPartition)
    {
        /* Close file before calling external formatter */
        fclose(File);

        /* Build mformat command */
        snprintf(FormatCommand, sizeof(FormatCommand), "mformat -i %s@@%ld -h32 -t32 -n64 -L32", FileName, (long)(Partition.StartLBA * SECTOR_SIZE));
        if(system(FormatCommand) != 0)
        {
            /* Failed to format partition */
            perror("Failed to format partition");
            return 1;
        }

        /* Reopen disk image */
        File = fopen(FileName, "r+b");
        if(!File) {
            /* Failed to open file */
            perror("Failed to reopen disk image");
            return 1;
        }
    }

    /* Write VBR to the start of the partition, if provided */
    if(VbrFile)
    {
        if (LoadSector(VbrFile, Vbr) != 0) {
            fclose(File);
            return 1;
        }
        /* Seek to the start of the partition and write VBR */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fwrite(Vbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to write VBR to disk image */
            perror("Failed to write VBR to disk image");
            fclose(File);
            return 1;
        }
    }

    fclose(File);
    printf("Successfully created disk image '%s' (%ld MB) with one bootable W95 FAT32 partition%s%s.\n",
           FileName,
           DiskSizeMB,
           MbrFile ? " with MBR from file" : "",
           VbrFile ? " and VBR from file" : "");
    return 0;
}
