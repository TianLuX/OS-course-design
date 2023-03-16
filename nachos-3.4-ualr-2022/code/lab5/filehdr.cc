// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include <time.h>
#include <string.h>
#include "copyright.h"
#include "system.h"
#include "filehdr.h"
#include "filesys.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------


FileHeader::FileHeader() {
    memset(dataSectors, 0, sizeof(dataSectors));
    nextSectorDirectory=-1;
}
/// @brief 
/// @param freeMap 
/// @param fileSize 
/// @return 
bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{   this->numBytes=fileSize;
   
    int numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space
    bool isNeedTwoIndex=0;
    char* nextSector;
    //judge if need second level index
    if (numSectors>NumDirect)
        {
            isNeedTwoIndex=1;
            nextSector=new char[SectorSize];
            nextSectorDirectory=freeMap->Find();
            synchDisk->ReadSector(nextSectorDirectory,nextSector);
        }else{
            nextSectorDirectory=-1;
        }
    int *data2=(int*)nextSector;
    for (int i = 0; i < numSectors; i++)
	{   if (i<NumDirect)
            dataSectors[i] = freeMap->Find();
        else{
            data2[i-NumDirect]=freeMap->Find();
        }
    };
    printf("allocate: ");
     for (int i = 0; i < numSectors; i++){
          if (i<NumDirect)
            printf("%d\t", dataSectors[i] );
        else{
             printf("data2:%d\t", data2[i-NumDirect] );
        }
     }
    printf("\n");
    if (isNeedTwoIndex){
        synchDisk->WriteSector(nextSectorDirectory,nextSector);
        delete[] nextSector;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    int numSectors  = divRoundUp(numBytes, SectorSize);
    char *twoIndex=new char[SectorSize];
    int* n=(int*)twoIndex;
    if (nextSectorDirectory!=-1)
    {
        synchDisk->ReadSector(nextSectorDirectory,twoIndex);

    }
    for (int i = 0; i < numSectors; i++) {
	if(i<NumDirect){
        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	    freeMap->Clear((int) dataSectors[i]);
    }
    else{
        freeMap->Clear((int) n[i-NumDirect]);
    }} 
    if (nextSectorDirectory!=-1){
        delete[] twoIndex;
        freeMap->Clear(nextSectorDirectory);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{   
    // int *n=(int *)this;
    // printf("this:");
    // for (int i=0;i<SectorSize/4;i++)
    // printf(" %d \n",n[i]);
    //printHeader();
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

/// @brief get Sector number by offset
/// @param offset 
/// @return Sector number
int
FileHeader::ByteToSector(int offset)
{   int trueSectorNumber;
    //
   // ASSERT(offset<getNumBytes());
  
    if (offset/SectorSize< NumDirect)
        //in 29 1-level index 
        trueSectorNumber=(dataSectors[offset / SectorSize]);
    else{
        //in 2-level index   
        char* twoLevelIndex=new char[SectorSize];

        synchDisk->ReadSector(nextSectorDirectory,twoLevelIndex);
        int *data=(int *)twoLevelIndex;
        int indexIn2Level=(offset/SectorSize)-NumDirect;        
        // for (int i=0;i++;i<sizeof(int))
        // {
        //     trueSectorNumber+=(twoLevelIndex[indexIn2Level+(sizeof(int)-i-1)]<<8*i);
        // }
        trueSectorNumber=data[indexIn2Level];
        delete twoLevelIndex;
    }
   //  printf("\nbyteTosector:%d\n",trueSectorNumber);
    return trueSectorNumber;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    //++++++++++++++cl add++++++++++++++
    time_t timep = this->time;
    if (this->time == 0) {
        printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    } else {
        char *s = ctime(&timep);
        s[strlen(s) - 1] = '\0';
        printf("FileHeader contents.  File size: %d.  File modification time: %s.  "
        "File blocks:\n", numBytes, s);
    }
    //++++++++++++++cl add++++++++++++++
    if (nextSectorDirectory!=-1){

    }
    int numSectors = divRoundUp(numBytes, SectorSize);
    char *twoIndex=new char[SectorSize];
    int* n=(int*)twoIndex;
    if (nextSectorDirectory!=-1)
    {
        synchDisk->ReadSector(nextSectorDirectory,twoIndex);

    }
    n=(int *)twoIndex;
    for (i = 0; i < numSectors; i++)
	{   if(i<NumDirect)
        printf("%d ", dataSectors[i]);
        else
        printf("%d ",n[i-NumDirect]);
    }
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
    if(i<NumDirect)
	synchDisk->ReadSector(dataSectors[i], data);
    else
    synchDisk->ReadSector(n[i-NumDirect], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}
//after extend() ,we need  writeBack() head 
bool FileHeader::extend(int newLength) {

    // 计算之前和所需的扇区数量
    int new_sectors_num = divRoundUp(newLength, SectorSize);
    int pre_sectors_num = divRoundUp(numBytes, SectorSize);

    if(new_sectors_num > pre_sectors_num){
        int extraSector = new_sectors_num - pre_sectors_num;
        bool isNeed=0;
        //test if need secondIndex    
        if (new_sectors_num>NumDirect && pre_sectors_num<=NumDirect){
            extraSector++;
            isNeed=1;
        }    
        // 从磁盘中取出位图
        OpenFile *bitmapfile = new OpenFile(FreeMapSector);
        BitMap *freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(bitmapfile);
       // printf("\n%d is need %d,free:%d\n",isNeed,extraSector,freeMap->NumClear());
        
        // 判断空间是否够用，文件头只有一个header,
        ASSERT(freeMap->NumClear()>extraSector);
        // printf("\n%d is need %d,free:%d\n",isNeed,extraSector,freeMap->NumClear());
        //need secondIndex
        if (isNeed)
        {
            
            nextSectorDirectory=freeMap->Find();
            // printf("\n xia yi ge %d\n",nextSectorDirectory);
            for(int i = pre_sectors_num; i<NumDirect; i++) {
            dataSectors[i] = freeMap->Find();
            }
            int* data2Index=new int[new_sectors_num-NumDirect];
            for(int i=0;i<new_sectors_num-NumDirect;i++){
                data2Index[i]=freeMap->Find();
            }

            synchDisk->WriteSector(nextSectorDirectory,(char *)data2Index);
            delete[]data2Index;
        }//had secondIndex
        else if  (pre_sectors_num>NumDirect){

            int* data2Index=new int[SectorSize/4];
            synchDisk->ReadSector(nextSectorDirectory,(char *)data2Index);
            for(int i=pre_sectors_num-NumDirect;i<new_sectors_num-NumDirect;i++){
                data2Index[i]=freeMap->Find();
                //printf("%d hao :%d",data2Index[i],i);
            }
            // printf(",,%d",freeMap->Test(41));
            // printf("pre %d,new %d \n ",pre_sectors_num-NumDirect,new_sectors_num-NumDirect);
            // for(int i=0;i<SectorSize/4;i++){
            //     printf("i:%d ,, %d",i,data2Index[i]);
            // }
            
            synchDisk->WriteSector(nextSectorDirectory,(char *)data2Index);
            delete[]data2Index;
        //withoutSecondIndex
        }else{
            for(int i = pre_sectors_num; i<new_sectors_num; i++) {
                dataSectors[i] = freeMap->Find();
            }
        }
        // 存储bitmap
        freeMap->WriteBack(bitmapfile);
        delete freeMap;
        delete bitmapfile;  
    }

    // 修改长度
    numBytes = newLength;
    return true;
}

//+++++++++++cl add+++++++++++
int 
FileHeader::getNumBytes() {
    return this->numBytes;
}

void 
FileHeader::setTime(int time) {
    this->time = time;
}

int
FileHeader::getTime() {
    return this->time;
}

void 
FileHeader::printHeader(){
        printf("\nnumbytes %d\n sectors:",numBytes);
      for (int i = 0; i < NumDirect; i++)
      {
        printf("%d,",dataSectors[i]);
      }
      printf("\n");
      if(nextSectorDirectory!=-1){
        printf("next %d\n ",nextSectorDirectory);
        char*data=new char[SectorSize];
        synchDisk->ReadSector(nextSectorDirectory, data);
        int *n=(int *)data;
        for (int i=0;i<SectorSize/4;i++){
          printf("%d, ",n[i]);
        }
      }
}