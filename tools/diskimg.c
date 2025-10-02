/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/diskimg.c
 * DESCRIPTION: Disk Image manipulation tool
 * DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
 *              Aiken Harris <harraiken91@gmail.com>
 */

#include "xtchain.h"


typedef struct _RESERVED_SECTOR_INFO
{
    int SectorNumber;
    const char* Description;
} RESERVED_SECTOR_INFO, *PRESERVED_SECTOR_INFO;

static RESERVED_SECTOR_INFO Fat32ReservedMap[] =
{
    {0, "Main VBR"},
    {1, "FSInfo Sector"},
    {6, "Backup VBR"},
    {7, "Backup FSInfo Sector"},
    {-1, NULL}
};

/* Forward references */
static void CopyData(const char *Image, long Offset, const char *SourceDir, const char *Relative);
static void CopyImageFile(const char *Image, long Offset, const char *SourceFile, const char *Relative);
static long DetermineExtraSector(long sectors_to_write);
long GetFileSize(const char *FileName);
int LoadSectors(const char *FileName, uint8_t *Buffer, int SectorCount);
static void MakeDirectory(const char *Image, long Offset, const char *Relative);

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

/* Determines a safe sector to write extra VBR data to */
static long DetermineExtraSector(long sectors_to_write)
{
    long Candidate;
    long Conflict;
    long Index;
    long LastSector;

    /* Start search from sector 1 (sector 0 is the main VBR) */
    for(Candidate = 1; Candidate < 32; Candidate++)
    {
        /* Calculate the last sector to write */
        LastSector = Candidate + sectors_to_write - 1;
        Conflict = 0;

        /* Check if it fits within the reserved region (32 sectors) */
        if(LastSector >= 32)
        {
            /* The remaining space is not large enough */
            break;
        }

        /* Check for conflicts with critical sectors */
        for(Index = 0; Fat32ReservedMap[Index].SectorNumber != -1; Index++)
        {
            if(Candidate <= Fat32ReservedMap[Index].SectorNumber && LastSector >= Fat32ReservedMap[Index].SectorNumber)
            {
                /* Found a conflict */
                Conflict = 1;
                break;
            }
        }

        /* Make sure there are no conflicts */
        if(!Conflict)
        {
            /* Found a suitable slot */
            return Candidate;
        }
    }

    /* No suitable slot found */
    return -1;
}

/* Gets the size of a file */
long GetFileSize(const char *FileName)
{
    FILE *File;
    long Size;

    /* Open the file in binary mode */
    File = fopen(FileName, "rb");
    if(!File)
    {
        /* Failed to open file */
        perror("Failed to open file for size check");
        return -1;
    }

    /* Get the file size */
    fseek(File, 0, SEEK_END);
    Size = ftell(File);

    /* Close the file and return the size */
    fclose(File);
    return Size;
}

/* Loads one or more sectors from a file */
int LoadSectors(const char *FileName, uint8_t *Buffer, int SectorCount)
{
    FILE *File;
    long FileSize;
    long BytesToRead = SectorCount * SECTOR_SIZE;

    /* Get and validate file size */
    FileSize = GetFileSize(FileName);
    if(FileSize < 0)
    {
        /* Failed to get file size */
        perror("Failed to get file size");
        return -1;
    }
    if(FileSize != BytesToRead)
    {
        fprintf(stderr, "Error: file '%s' must be exactly %ld bytes, but is %ld bytes.\n", FileName, BytesToRead, FileSize);
        return -1;
    }

    /* Open the file in binary mode for reading */
    File = fopen(FileName, "rb");
    if(!File) {
        /* Failed to open file */
        perror("Failed to open sector file for reading");
        return -1;
    }

    /* Read sectors to buffer */
    if(fread(Buffer, 1, BytesToRead, File) != BytesToRead)
    {
        /* Failed to read sectors */
        perror("Failed to read sectors from file");
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
    long Index;
    long FatFormat = 32;
    char FormatCommand[512];
    long FormatPartition = 0;
    long DiskSizeBytes = 0;
    long DiskSizeMB = 0;
    long SectorsToWrite = 0;
    long VbrExtraSector = -1;
    long VbrFileSize = -1;
    long VbrTotalSectors = 0;
    long VbrLastSector = 99;
    char VbrInfo[128] = "";
    MBR_PARTITION Partition = {0};
    char Zero[SECTOR_SIZE] = {0};
    uint8_t Mbr[SECTOR_SIZE] = {0};
    uint8_t ImageVbr[SECTOR_SIZE * 2] = {0};
    uint8_t *FullVbr = NULL;
    const char *FileName = NULL;
    const char *MbrFile = NULL;
    const char *VbrFile = NULL;
    const char *CopyDir = NULL;

    /* Parse command line arguments */
    for(Index = 1; Index < argc; Index++)
    {
        if(strcmp(argv[Index], "-c") == 0 && Index + 1 < argc)
        {
            /* Copy directory */
            CopyDir = argv[++Index];
        }
        else if(strcmp(argv[Index], "-e") == 0 && Index + 1 < argc)
        {
            /* VBR extra data sector */
            VbrExtraSector = atol(argv[++Index]);
        }
        else if(strcmp(argv[Index], "-f") == 0 && Index + 1 < argc)
        {
            /* Format partition */
            FormatPartition = 1;
            FatFormat = atoi(argv[++Index]);
            if(FatFormat != 16 && FatFormat != 32)
            {
                fprintf(stderr, "Error: FAT format (-f) must be 16 or 32\n");
                return 1;
            }
        }
        else if(strcmp(argv[Index], "-m") == 0 && Index + 1 < argc)
        {
            /* MBR file */
            MbrFile = argv[++Index];
        }
        else if(strcmp(argv[Index], "-o") == 0 && Index + 1 < argc)
        {
            /* Output file */
            FileName = argv[++Index];
        }
        else if(strcmp(argv[Index], "-s") == 0 && Index + 1 < argc)
        {
            /* Disk size */
            DiskSizeMB = atol(argv[++Index]);
        }
        else if(strcmp(argv[Index], "-v") == 0 && Index + 1 < argc)
        {
            /* VBR file */
            VbrFile = argv[++Index];
        }
        else
        {
            /* Unknown argument */
            fprintf(stderr, "Unknown argument: %s\n", argv[Index]);
            return 1;
        }
    }

    /* Check for required arguments */
    if(DiskSizeMB <= 0 || FileName == NULL)
    {
        /* Missing required arguments, print usage */
        fprintf(stderr, "Usage: %s -o <output.img> -s <size_MB> [-b <sector>] [-c <dir>] [-f 16|32] [-m <mbr.img>] [-v <vbr.img>]\n", argv[0]);
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
    for(Index = 0; Index < DiskSizeBytes / SECTOR_SIZE; Index++)
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
        if(LoadSectors(MbrFile, Mbr, 1) != 0)
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
        /* Failed to write MBR to disk image */
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

        /* Format the partition */
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

        /* Check FAT format */
        if(FatFormat == 32)
        {
            /* For FAT32, TotalSectors32 must be set */
            *(uint16_t*)&ImageVbr[0x13] = 0;
            if(*(uint32_t*)&ImageVbr[0x20] == 0)
            {
                /* Mformat did not set the field, update it */
                memcpy(&ImageVbr[0x20], &Partition.Size, sizeof(uint32_t));
            }
        }
        else
        {
            /* For FAT16, check if parition size exceeds 65535 sectors */
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
                *(uint16_t*)&ImageVbr[0x13] = 0;
                if(*(uint32_t*)&ImageVbr[0x20] == 0)
                {
                    /* Mformat did not set the field, update it */
                    memcpy(&ImageVbr[0x20], &Partition.Size, sizeof(uint32_t));
                }
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
        VbrFileSize = GetFileSize(VbrFile);
        if(VbrFileSize < 0)
        {
            /* The GetFileSize function already prints a perror message */
            perror("Could not get size of VBR file\n");
            return 1;
        }

        /* Check if VBR file size is a multiple of sector size */
        if(VbrFileSize % SECTOR_SIZE != 0)
        {
            /* Unable to determine VBR file size */
            perror("Failed to determine VBR file size");
            return 1;
        }

        VbrTotalSectors = VbrFileSize / SECTOR_SIZE;

        /* Allocate memory for the entire VBR file */
        FullVbr = malloc(VbrFileSize);
        if(!FullVbr)
        {
            /* Memory allocation failed */
            perror("Failed to allocate memory for VBR file");
            return 1;
        }

        /* Read the entire VBR file into the buffer */
        if(LoadSectors(VbrFile, FullVbr, VbrTotalSectors) != 0)
        {
            /* Failed to load VBR from file */
            perror("Failed to load VBR from file");
            free(FullVbr);
            return 1;
        }

        /* Read the existing VBR from the formatted partition to get the correct BPB */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fread(ImageVbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to read VBR from disk image */
            perror("Failed to read BPB from disk image");
            free(FullVbr);
            fclose(File);
            return 1;
        }

        /* Copy the BPB from the image's VBR to VBR buffer */
        if(FatFormat == 32)
        {
            /* For FAT32, BPB is larger (up to offset 89) */
            memcpy(&FullVbr[3], &ImageVbr[3], 87);
        }
        else
        {
            /* For FAT16, BPB is smaller (up to offset 61) */
            memcpy(&FullVbr[3], &ImageVbr[3], 59);
        }

        /* Write the first 512 bytes of the final, merged VBR to the start of the partition */
        fseek(File, Partition.StartLBA * SECTOR_SIZE, SEEK_SET);
        if(fwrite(FullVbr, 1, SECTOR_SIZE, File) != SECTOR_SIZE)
        {
            /* Failed to write VBR to disk image */
            perror("Failed to write VBR to disk image");
            free(FullVbr);
            fclose(File);
            return 1;
        }

        /* Handle extra VBR data if it exists */
        if(FatFormat == 32)
        {
            /* Check if there is extra VBR data to write */
            if(VbrTotalSectors > 1)
            {
                /* Check if extra sector has been provided by the user */
                if(VbrExtraSector == -1)
                {
                    /* Determine a safe sector to write extra VBR data to */
                    long sectors_to_write = VbrTotalSectors - 1;
                    VbrExtraSector = DetermineExtraSector(sectors_to_write);
                    if(VbrExtraSector == -1)
                    {
                        /* Failed to find a safe sector */
                        fprintf(stderr, "Error: Could not automatically find a safe space in the FAT32 reserved region for %ld extra VBR sectors.\n",
                                sectors_to_write);
                        free(FullVbr);
                        return 1;
                    }
                }

                /* Calculate number of sectors and last sector to write */
                SectorsToWrite = VbrTotalSectors - 1;
                VbrLastSector = VbrExtraSector + SectorsToWrite - 1;

                /* Ensure VBR will not be writen outside the reserved region (32 sectors for FAT32) */
                if(VbrLastSector >= 32)
                {
                    /* The remaining space is not large enough to fit the extra VBR data */
                    fprintf(stderr, "Error: VBR file is too large. Writing to sector %ld would exceed the FAT32 reserved region (32 sectors).\n", VbrLastSector);
                    free(FullVbr);
                    return 1;
                }

                /* Safety check: ensure we do not overwrite critical sectors */
                for(Index = 0; Fat32ReservedMap[Index].SectorNumber != -1; Index++)
                {
                    /* Check if we are about to overwrite a critical sector */
                    if(VbrExtraSector <= Fat32ReservedMap[Index].SectorNumber && VbrLastSector >= Fat32ReservedMap[Index].SectorNumber)
                    {
                        /* We are about to overwrite a critical sector */
                        fprintf(stderr, "Error: Writing VBR extra data would overwrite critical sector %d (%s).\n",
                                Fat32ReservedMap[Index].SectorNumber, Fat32ReservedMap[Index].Description);
                        free(FullVbr);
                        return 1;
                    }
                }

                /* Write the rest of the VBR data */
                fseek(File, (Partition.StartLBA + VbrExtraSector) * SECTOR_SIZE, SEEK_SET);
                if(fwrite(FullVbr + SECTOR_SIZE, 1, SectorsToWrite * SECTOR_SIZE, File) != (size_t)(SectorsToWrite * SECTOR_SIZE))
                {
                    /* Failed to write extra VBR data to disk image */
                    perror("Failed to write extra VBR data to disk image");
                    free(FullVbr);
                    fclose(File);
                    return 1;
                }
            }
        }
        else /* FatFormat == 16 */
        {
            /* Check if there is extra VBR data to write */
            if(VbrTotalSectors > 1)
            {
                /* FAT16 only supports a 1-sector VBR */
                fprintf(stderr, "Error: VBR file is %ld sectors, but FAT16 only supports a 1-sector VBR.\n", VbrTotalSectors);
                free(FullVbr);
                return 1;
            }
        }

        /* Free allocated memory */
        free(FullVbr);
    }

    /* Close file */
    fclose(File);

    /* Copy files if requested */
    if(CopyDir)
    {
        CopyData(FileName, (long)(Partition.StartLBA * SECTOR_SIZE), CopyDir, "");
    }

    /* Check if VBR was written */
    if(VbrFile)
    {
        /* Compose VBR info string */
        if(VbrExtraSector != -1)
        {
            /* VBR with extra data */
            snprintf(VbrInfo, sizeof(VbrInfo), ", VBR written (extra data at sector %ld)", VbrExtraSector);
        }
        else
        {
            /* Standard VBR */
            snprintf(VbrInfo, sizeof(VbrInfo), ", VBR written");
        }
    }

    /* Print success message */
    printf("Successfully created disk image '%s' (%ld MB) with bootable W95 FAT-%ld partition%s%s%s.\n",
           FileName,
           DiskSizeMB,
           FatFormat,
           MbrFile ? ", MBR written" : "",
           VbrInfo,
           CopyDir ? ", files copied" : "");
    return 0;
}
