/**
 * PROJECT:     XTchain
 * LICENSE:     See COPYING.md in the top level directory
 * FILE:        tools/bin2c.c
 * DESCRIPTION: Binary to C converter
 * DEVELOPERS:  Rafal Kupiec <belliash@codingworkshop.eu.org>
 */

#include "xtchain.h"

int main(int argc, char *argv[])
{
    /* Check for proper number of arguments */
    if(argc != 4)
    {
        printf("Usage: %s <input binary> <output file> <structure name>\n", argv[0]);
        return 1;
    }

    /* Open the input binary file in binary mode */
    FILE *inputFile = fopen(argv[1], "rb");
    if(inputFile == NULL)
    {
        printf("Error: unable to open file %s\n", argv[1]);
        return 1;
    }

    /* Open the destination source code file in text mode */
    FILE *outputFile = fopen(argv[2], "w");
    if(outputFile == NULL)
    {
        printf("Error: unable to open file %s\n", argv[2]);
        fclose(inputFile);
        return 1;
    }

    /* Get the size of the binary file */
    fseek(inputFile, 0, SEEK_END);
    long binSize = ftell(inputFile);
    rewind(inputFile);

    /* Allocate memory for the binary data */
    unsigned char *binData = (unsigned char *)malloc(binSize);
    if(binData == NULL)
    {
        printf("Error: unable to allocate memory for binary data\n");
        fclose(inputFile);
        fclose(outputFile);
        return 1;
    }

    /* Read the binary data into memory */
    fread(binData, sizeof(unsigned char), binSize, inputFile);

    /* Write the C structure to the header file */
    fprintf(outputFile, "unsigned char %s[] = {", argv[3]);
    for(int i = 0; i < binSize; i++)
    {
        fprintf(outputFile, "0x%02X", binData[i]);
        if(i < binSize - 1)
        {
            fprintf(outputFile, ",");
        }
    }
    fprintf(outputFile, "};\nunsigned int %s_size = %ld;\n", argv[3], binSize);
    free(binData);

    /* Close all open files */
    fclose(inputFile);
    fclose(outputFile);

    printf("Binary data converted to C structure successfully.\n");
    return 0;
}
