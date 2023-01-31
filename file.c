#include "file.h"
#include "inode.h"
#include "parameters.h"
#include "builder.h"
#include "LibDisk.h"
#include "directory.h"
#include <string.h>

int addFile(int parentInode, char* fileName)
{
    printf("filename : %s\n", fileName);
    printf("parentInode : %d\n", parentInode);
    // Get an inode block
    int inodeIndex = FindNextAvailableInodeBlock();
    if(inodeIndex < 0)
        return -1;

    // Get a data block
    int dataIndex = FindNextAvailableDataBlock();
    if(dataIndex < 0)
        return -1;

    // update bitmap blocks of inode and data blocks
    ChangeInodeBitmapStatus(inodeIndex, OCCUPIED);
    ChangeDataBitmapStatus(dataIndex, OCCUPIED);

    char* inodeBlock = calloc(sizeof(char), SECTOR_SIZE/INODE_PER_BLOCK_NUM);
    BuildInode(inodeBlock, FILE_ID);
    InitializeFileInode(inodeBlock, dataIndex);

    // check if failed to write inode in disk
    if(WriteInodeInSector(inodeIndex, inodeBlock) == -1)
    {
        free(inodeBlock);
        return -1;
    }

    free(inodeBlock);

    char* dataBlock = calloc(sizeof(char), SECTOR_SIZE);

    // check if failed to write data block in disk
    if(Disk_Write(DATA_FIRST_BLOCK_INDEX + dataIndex, dataBlock) == -1)
    {
        printf("Failed to write datablock to disk");
        free(dataBlock);
        return -1;
    }

    DirectoryEntry * dirEntry = (DirectoryEntry *) malloc ( sizeof(DirectoryEntry));
    dirEntry->inodePointer = inodeIndex;
    strcpy(dirEntry->pathName, fileName);

    if(addDirectoryEntry(parentInode, dirEntry) != 0)
    {
        free(dirEntry);
        return -1;
    }

    // file create successfully...
    free(dataBlock);
    free(dirEntry);
    return 0;
}
