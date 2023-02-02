#include "file.h"
#include "inode.h"
#include "parameters.h"
#include "builder.h"
#include "LibDisk.h"
#include "directory.h"
#include "LibFS.h"
#include <string.h>
#include <stdio.h>
#include "LibDisk.h"

// the fileTable in memory (static makes it private to the file)
static FileTableEntry* fileTable;

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

int CreateFileTable()
{
    fileTable = (FileTableEntry *) calloc(OPEN_FILE_NUM_MAX, sizeof(FileTableEntry));
    if(fileTable == NULL)
        return -1;
    return 0;
}

int getAvailabeFileDescriptor()
{
    int i;
    for(i = 0; i < OPEN_FILE_NUM_MAX; i++)
    {
        if(fileTable[i].isValid == NOT_VALID)
            return i;
    }
    return -1;
}

int getInodePointerOfFileEntry(int fd)
{
    if(fd < 0 || fd > OPEN_FILE_NUM_MAX)
        return -1;
    return fileTable[fd].inodePointer;
}

int updateFilePointerOfFileEntry(int fd, int filePointer)
{
    if(fd < 0 || fd > OPEN_FILE_NUM_MAX)
        return -1;
    fileTable[fd].filePointer = filePointer;
    return 0;
}

int SizeOfFile(int inodeNumber)
{
    char* inodeBuffer=calloc(sizeof(char),INODE_SIZE);
    char* inodeSegmentPointerToSector =calloc(sizeof(char),sizeof(int));
    char* sectorBuffer=calloc(sizeof(char),SECTOR_SIZE);
    char* sizeBuffer=calloc(sizeof(char),sizeof(int));
    
    int inodePointerToSectorNumber;
    int fileSize;
    
    // Read the inode
    if( ReadInode(inodeNumber, inodeBuffer) == -1)
    {
        printf("Disk failed to read inode block\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        free(sizeBuffer);
        return -1;
    }
    
    // Check that inode is File  FILE_ID=0x80 , DIRECORY_ID=0x00
    if (!(inodeBuffer[0] & FILE_ID))
    {
        printf("Inode is not directory, it is file.\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        free(sizeBuffer);
        return -1;
    }
    
    //find appropriate sector which shows the size
    memcpy((void*)inodeSegmentPointerToSector,(void*)inodeBuffer+META_DATA_PER_INODE_BYTE_NUM,sizeof(int));
    inodePointerToSectorNumber=StringToInt(inodeSegmentPointerToSector);
        
    //read the appropriate sector and write in sectorBuffer
    if( Disk_Read(DATA_FIRST_BLOCK_INDEX + inodePointerToSectorNumber, sectorBuffer) == -1)
    {
        printf("Disk failed to read sector block\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        free(sizeBuffer);

        return -1;
    }
    
    //find size and convert it into int
    memcpy((void*)sizeBuffer,(void*)sectorBuffer,sizeof(int));
    fileSize=StringToInt(sizeBuffer);
    
    free(inodeBuffer);
    free(inodeSegmentPointerToSector);
    free(sectorBuffer);
    free(sizeBuffer);

    return fileSize;
}

int DataBlocksOccupiedByFile(int inodeNumber,int* sectorNumbers)
{
    char* inodeBuffer=calloc(sizeof(char),INODE_SIZE);
    char* inodeSegmentPointerToSector =calloc(sizeof(char),sizeof(int));
    
    int inodePointerToSectorNumber;
    int i;
    
    
    int entryOccupiedNumber;
    int fileSize=0;
    
    // Find size of file
    fileSize=SizeOfFile(inodeNumber);
    
    if(fileSize == -1)
    {
        return -1;
    }
    
    // Number of occupied entry by file
    entryOccupiedNumber=min(SECTOR_PER_FILE_MAX,(fileSize+sizeof(int)-1)/SECTOR_SIZE+1);
    
    // Read the inode
    if( ReadInode(inodeNumber, inodeBuffer) == -1)
    {
        printf("Disk failed to read inode block\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        return -1;
    }
    
    //find appropriate sector and looking for search word
    for (i=0;i<entryOccupiedNumber;i++)
    {
        //find sector number
        memcpy((void*)inodeSegmentPointerToSector,(void*)inodeBuffer+META_DATA_PER_INODE_BYTE_NUM+i*sizeof(int),sizeof(int));
        inodePointerToSectorNumber=StringToInt(inodeSegmentPointerToSector);
        
        // add this sector numbers and increase counter
        sectorNumbers[i]=inodePointerToSectorNumber;
        
    }
    
    free(inodeBuffer);
    free(inodeSegmentPointerToSector);
    return entryOccupiedNumber;
    
}

int openFileDescriptor(char *path)
{
    int fd = getAvailabeFileDescriptor();
    if(fd < 0)
    {
        osErrno = E_TOO_MANY_OPEN_FILES;
        return -1;
    }

    // allocate memory for storing string...
    char* array[128];
    char myPath[256];

    // make a copy of path to modify
    strcpy(myPath, path);

    // tokenize path and make array of path elements...
    int i = BreakPathName(myPath, array);

    int parent;
    int current;

    if(findLeafInodeNumber(myPath, array, i, &parent, &current, 0) != 0)
    {
        osErrno = E_NO_SUCH_FILE;
        return -1;
    }

    fileTable[fd].fileDescriptor = fd;
    fileTable[fd].inodePointer = current;
    fileTable[fd].filePointer = 0;
    fileTable[fd].sizeOfFile = SizeOfFile(current);
    fileTable[fd].isValid = VALID;
    strcpy(fileTable[fd].fileName, array[i-1]);
    strcpy(fileTable[fd].filePath, path);

    return fd;
}

int isFileOpen(int fd)
{
    if(fd < 0 || fd > OPEN_FILE_NUM_MAX)
        return -1;
    if(fileTable[fd].isValid == VALID)
        return 0;
    return -1;
}

int removeFileTableEntry(int fd)
{
    if(fd < 0 || fd > OPEN_FILE_NUM_MAX)
        return -1;
    fileTable[fd].isValid = NOT_VALID;
    return 0;
}

int FileRead(int fd, char *buffer, int size){
    char* inodeBuffer=calloc(sizeof(char),INODE_SIZE);
    char* inodeSegmentPointerToSector =calloc(sizeof(char),sizeof(int));
    char* sectorBuffer=calloc(sizeof(char),SECTOR_SIZE);
    
    // determine how many bytes we must read
    int readSize;
    if(fileTable[fd].sizeOfFile-fileTable[fd].filePointer<0)
    {
       readSize=0;
    }
    else
    {
       readSize=min(fileTable[fd].sizeOfFile-fileTable[fd].filePointer,size);
    }
    int transferedSize=0;
    
    int inodePointerToSectorNumber;
    int i;
    
    // Number of occupied entry by file
    int entryOccupiedNumber=min(SECTOR_PER_FILE_MAX,(fileTable[fd].sizeOfFile+sizeof(int)-1)/SECTOR_SIZE+1);
    
    // Read the inode
    if( ReadInode(fileTable[fd].inodePointer, inodeBuffer) == -1)
    {
        printf("Disk failed to read inode block\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        return -1;
    }
    
    //find appropriate sectors and reading from them and save into buffer
    for (i=0;i<entryOccupiedNumber;i++)
    {
        //find sector number
        memcpy((void*)inodeSegmentPointerToSector,(void*)inodeBuffer+META_DATA_PER_INODE_BYTE_NUM+i*sizeof(int),sizeof(int));
        inodePointerToSectorNumber=StringToInt(inodeSegmentPointerToSector);
        
        //read the appropriate sector and write in sectorBuffer
        if( Disk_Read(DATA_FIRST_BLOCK_INDEX + inodePointerToSectorNumber, sectorBuffer) == -1)
        {
            printf("Disk failed to read sector block\n");
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return -1;
        }
        
        //transfering data
        if (i==0 && fileTable[fd].filePointer<(SECTOR_SIZE-sizeof(int)))
        {
            memcpy(buffer+transferedSize,sectorBuffer+sizeof(int)+fileTable[fd].filePointer,min(SECTOR_SIZE-sizeof(int),readSize));
            transferedSize=transferedSize + min(SECTOR_SIZE-sizeof(int),readSize);
            fileTable[fd].filePointer=fileTable[fd].filePointer+min(SECTOR_SIZE-sizeof(int),readSize);
        }
        else if (i==((fileTable[fd].filePointer+sizeof(int))/SECTOR_SIZE))
        {
            memcpy(buffer+transferedSize,sectorBuffer+((fileTable[fd].filePointer+sizeof(int))%SECTOR_SIZE),
                   min(SECTOR_SIZE,readSize-transferedSize));
            transferedSize=transferedSize+min(SECTOR_SIZE,readSize-transferedSize);
            fileTable[fd].filePointer=fileTable[fd].filePointer+min(SECTOR_SIZE,readSize-transferedSize);
        }
        
        if(transferedSize==readSize)
        {
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return transferedSize;
        }
        
        if(transferedSize>readSize)
        {
            printf("Some error in transfering data\n");
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return -1;
        }
        
    }
    
    free(inodeBuffer);
    free(inodeSegmentPointerToSector);
    free(sectorBuffer);
    return transferedSize;
    
}


//return -1 for errors
int FileWrite(int fd, char* buffer , int size)
{
    char* inodeBuffer=calloc(sizeof(char),INODE_SIZE);
    char* inodeSegmentPointerToSector =calloc(sizeof(char),sizeof(int));
    char* sectorBuffer=calloc(sizeof(char),SECTOR_SIZE);
    
    // determine how many bytes we must read
    int transferedSize=0;
    int newSizeOfFile;
    
    int inodePointerToSectorNumber;
    int i;
    
    //determine new size of file
    newSizeOfFile=max(fileTable[fd].sizeOfFile,fileTable[fd].filePointer+size);
    if(newSizeOfFile>SECTOR_PER_FILE_MAX*SECTOR_SIZE-sizeof(int))
    {
        printf("There is not enough space to write\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        return -1;
    }
    
    // Read the inode
    if( ReadInode(fileTable[fd].inodePointer, inodeBuffer) == -1)
    {
        printf("Disk failed to read inode block\n");
        free(inodeBuffer);
        free(inodeSegmentPointerToSector);
        free(sectorBuffer);
        return -1;
    }
    
    //find appropriate sectors and reading from them and save into buffer
    for (i=0;i<SECTOR_PER_FILE_MAX;i++)
    {
        //find sector number
        memcpy((void*)inodeSegmentPointerToSector,(void*)inodeBuffer+META_DATA_PER_INODE_BYTE_NUM+i*sizeof(int),sizeof(int));
        inodePointerToSectorNumber=StringToInt(inodeSegmentPointerToSector);
        
        //read the appropriate sector and write in sectorBuffer
        if( Disk_Read(DATA_FIRST_BLOCK_INDEX + inodePointerToSectorNumber, sectorBuffer) == -1)
        {
            printf("Disk failed to read sector block\n");
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return -1;
        }
        
        // write the new size in appropriate part
        if ( i== 0)
        {
            sectorBuffer=newSizeOfFile;
        }
        
        //transfering data
        if (i==0 && fileTable[fd].filePointer<(SECTOR_SIZE-sizeof(int)))
        {
            memcpy(sectorBuffer+sizeof(int)+fileTable[fd].filePointer,buffer+transferedSize,min(SECTOR_SIZE-sizeof(int),size));
            transferedSize=transferedSize + min(SECTOR_SIZE-sizeof(int),size);
            fileTable[fd].filePointer=fileTable[fd].filePointer+min(SECTOR_SIZE-sizeof(int),size);
        }
        else if (i==((fileTable[fd].filePointer+sizeof(int))/SECTOR_SIZE))
        {
            memcpy(sectorBuffer+((fileTable[fd].filePointer+sizeof(int))%SECTOR_SIZE),buffer+transferedSize,min(SECTOR_SIZE,size-transferedSize));
            transferedSize=transferedSize+min(SECTOR_SIZE,size-transferedSize);
            fileTable[fd].filePointer=fileTable[fd].filePointer+min(SECTOR_SIZE,size-transferedSize);
        }
        
        if(transferedSize>size)
        {
            printf("Some error in transfering data. Note: some sectors have changed\n");
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return -1;
        }
        
        // write sectorBuffer in disk
        if( Disk_Write(DATA_FIRST_BLOCK_INDEX + inodePointerToSectorNumber, sectorBuffer) == -1)
        {
            printf("Disk failed to write changed block\n");
            osErrno=E_NO_SPACE;
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            return -1;
        }
        
        if(transferedSize==size)
        {
            free(inodeBuffer);
            free(inodeSegmentPointerToSector);
            free(sectorBuffer);
            fileTable[fd].sizeOfFile = newSizeOfFile;
            return transferedSize;
        }
        
    }
    
    free(inodeBuffer);
    free(inodeSegmentPointerToSector);
    free(sectorBuffer);
    fileTable[fd].sizeOfFile = newSizeOfFile;
    return transferedSize;
}

void printFileTableEntry(int fd)
{
    if(fileTable[fd].isValid == NOT_VALID)
        return;
    printf("|%-4d  |", fd);
    printf("%-8d  |", fileTable[fd].inodePointer);
    printf("%-8d  |", fileTable[fd].filePointer);
    printf("%-8d|", fileTable[fd].sizeOfFile);
    if(fileTable[fd].isValid == VALID)
        printf("V |");
    else
        printf("I |");
    printf("%-16s|", fileTable[fd].fileName);
    printf("%-24s|\n", fileTable[fd].filePath);
}

void printFileTable()
{
    int i;
    puts("\n+-----------------+----------+--------+--+----------------+------------------------+");
    puts("|fd    |inode     |fp        |fz      |V |filename        |filepath                |");
    puts("+------+----------+----------+--------+--+----------------+------------------------+");
    for(i = 0; i < OPEN_FILE_NUM_MAX; i++)
    {
        printFileTableEntry(i);
    }
    puts("+------+----------+----------+--------+--+----------------+------------------------+\n");
}
