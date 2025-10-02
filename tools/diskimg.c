/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/diskimg.c
 * DESCRIPTION: Disk Image manipulation tool
 * DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
 *              Aiken Harris <harraiken91@gmail.com>
 */

#include "xtchain.h"


/* Forward references */
static void CopyData(const char *Image, long Offset, const char *SourceDir, const char *Relative);
static void CopyImageFile(const char *Image, long Offset, const char *SourceFile, const char *Relative);
static void MakeDirectory(const char *Image, long Offset, const char *Relative);
int LoadSector(const char *FileName, uint8_t *Buffer);


/* Copies a directory recursively to the image */
static void CopyData(const char *Image, long Offset, const char *SourceDir, const char *Relative)
{
    char Path[2048];
    struct dirent *Entry;
    DIR *Directory;
    struct stat Stat;
    char new_rel[2048];

    /* Create the directory in the image */
    if(Relative[0] != '\0')
    {
        MakeDirectory(Image, Offset, Relative);
    }

    /* Open the source directory */
    Directory = opendir(SourceDir);
    if(!Directory)
    {
        /* Failed to open directory */
        perror("Failed to open source directory");
        return;
    }

    /* Read all entries in the directory */
    while((Entry = readdir(Directory)))
    {
        /* Skip . and .. entries */
        if(strcmp(Entry->d_name, ".") == 0 || strcmp(Entry->d_name, "..") == 0)
        {
            continue;
        }

        /* Build the full path to the entry */
        snprintf(Path, sizeof(Path), "%s%c%s", SourceDir, PATH_SEP, Entry->d_name);

        /* Stat the entry */
        if(stat(Path, &Stat) == -1)
        {
            /* Failed to stat entry */
            perror("Failed to stat file or directory");
            continue;
        }

        /* Build the relative path to the entry */
        if(Relative[0] != '\0')
        {
            snprintf(new_rel, sizeof(new_rel), "%s/%s", Relative, Entry->d_name);
        }
        else
        {
            snprintf(new_rel, sizeof(new_rel), "%s", Entry->d_name);
        }

        /* Copy the entry to the image */
        if(S_ISDIR(Stat.st_mode))
        {
            /* Entry is a directory, copy it recursively */
            CopyData(Image, Offset, Path, new_rel);
        }
        else if(S_ISREG(Stat.st_mode))
        {
            /* Entry is a file, copy it */
            CopyImageFile(Image, Offset, Path, new_rel);
        }
    }

    /* Close the directory */
    closedir(Directory);
}

/* Copies a file to the image */
static void CopyImageFile(const char *Image, long Offset, const char *SourceFile, const char *Relative)
{
    char Command[4096];

    /* Copy the file to the image */
    snprintf(Command, sizeof(Command), "mcopy -i \"%s@@%ld\" \"%s\" \"::/%s\"", Image, Offset, SourceFile, Relative);
    if(system(Command) != 0)
    {
        /* Failed to copy file */
        fprintf(stderr, "Faile to copy file '%s' to image\n", SourceFile);
    }
}

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

/* Creates a directory in the image */
static void MakeDirectory(const char *Image, long Offset, const char *Relative)
{
    char Command[4096];

    /* Create the directory in the image */
    snprintf(Command, sizeof(Command), "mmd -i \"%s@@%ld\" \"::/%s\"", Image, Offset, Relative);
    system(Command);
}

/* Main function */
int main(int argc, char **argv)
{
    FILE *File;
    long FatFormat = 32;
    char FormatCommand[512];
    long FormatPartition = 0;
    long DiskSizeBytes = 0;
    long DiskSizeMB = 0;
    MBR_PARTITION Partition = {0};
    char Zero[SECTOR_SIZE] = {0};
    uint8_t Mbr[SECTOR_SIZE] = {0};
    uint8_t Vbr[SECTOR_SIZE] = {0};
    uint8_t ImageVbr[SECTOR_SIZE] = {0};
    const char *FileName = NULL;
    const char *MbrFile = NULL;
    const char *VbrFile = NULL;
    const char *CopyDir = NULL;

    /* Parse command line arguments */
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            /* Copy directory */
            CopyDir = argv[++i];
        }
        else if(strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            /* Format partition */
            FormatPartition = 1;
            FatFormat = atoi(argv[++i]);
            if(FatFormat != 16 && FatFormat != 32)
            {
                fprintf(stderr, "Error: FAT format (-f) must be 16 or 32\n");
                return 1;
            }
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
        fprintf(stderr, "Usage: %s -o <output.img> -s <size_MB> [-c <dir>] [-f 16|32] [-m <mbr.img>] [-v <vbr.img>]\n", argv[0]);
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

    /* Setup MBR partition as W95 FAT16 or FAT32 */
    Partition.BootFlag = 0x80;
    Partition.Type = (FatFormat == 16) ? 0x06 : 0x0B;
    Partition.StartLBA = 2048;
    Partition.Size = (DiskSizeBytes / SECTOR_SIZE) - Partition.StartLBA;

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
        if(FatFormat == 16)
        {
            /* Format partition as FAT16 */
            snprintf(FormatCommand, sizeof(FormatCommand),
                     "mformat -i %s@@%ld",
                     FileName, (long)(Partition.StartLBA * SECTOR_SIZE));
        }
        else
        {
            /* Format partition as FAT32 */
            snprintf(FormatCommand, sizeof(FormatCommand),
                     "mformat -i %s@@%ld -F",
                     FileName, (long)(Partition.StartLBA * SECTOR_SIZE));
        }

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

        /* Read the VBR created by mformat */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fread(ImageVbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to read VBR */
            perror("Failed to read VBR from disk image");
            fclose(File);
            return 1;
        }

        /* Set the number of hidden sectors, as mformat sets it to 0 */
        if(*(uint32_t*)&ImageVbr[0x1C] == 0)
        {
            memcpy(&ImageVbr[0x1C], &Partition.StartLBA, sizeof(uint32_t));
        }

        /* Check if parition size exceeds 65535 sectors */
        if(Partition.Size < 65536)
        {
            /* Partition smaller than 32MB (65536 sectors), use 16-bit field TotalSectors16 */
            if(*(uint16_t*)&ImageVbr[0x13] == 0)
            {
                /* Mformat did not set the field, update it */
                memcpy(&ImageVbr[0x13], &((uint16_t){Partition.Size}), sizeof(uint16_t));
            }
        }
        else
        {
            /* Partition larger than 32MB (65536 sectors), use 32-bit field TotalSectors32 */
            if(*(uint32_t*)&ImageVbr[0x20] == 0)
            {
                /* Mformat did not set the field, update it */
                memcpy(&ImageVbr[0x20], &Partition.Size, sizeof(uint32_t));
            }
        }

        /* Write the corrected VBR back to the disk image */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fwrite(ImageVbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to write VBR */
            perror("Failed to write VBR to disk image");
            fclose(File);
            return 1;
        }
    }

    /* Write VBR to the start of the partition, if provided */
    if(VbrFile)
    {
        /* Read the VBR file into memory */
        if(LoadSector(VbrFile, Vbr) != 0)
        {
            fclose(File);
            return 1;
        }

        /* Read the existing VBR from the formatted partition to get the correct BPB */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fread(ImageVbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to read VBR from disk image */
            perror("Failed to read BPB from disk image");
            fclose(File);
            return 1;
        }

        /* Copy the BPB from the image's VBR to VBR buffer */
        if(FatFormat == 32)
        {
            /* For FAT32, BPB is larger (up to offset 89) */
            memcpy(&Vbr[3], &ImageVbr[3], 87);
        }
        else
        {
            /* For FAT16, BPB is smaller (up to offset 61) */
            memcpy(&Vbr[3], &ImageVbr[3], 59);
        }

        /* Write the final, merged VBR to the start of the partition */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fwrite(Vbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to write VBR to disk image */
            perror("Failed to write VBR to disk image");
            fclose(File);
            return 1;
        }
    }

    /* Close file */
    fclose(File);

    /* Copy files if requested */
    if(CopyDir)
    {
        CopyData(FileName, (long)(Partition.StartLBA * SECTOR_SIZE), CopyDir, "");
    }

    printf("Successfully created disk image '%s' (%ld MB) with bootable W95 FAT-%ld partition%s%s%s.\n",
           FileName,
           DiskSizeMB,
           FatFormat,
           MbrFile ? ", MBR written" : "",
           VbrFile ? ", VBR written" : "",
           CopyDir ? ", files copied" : "");
    return 0;
}
