#pragma once

#include "HDTools.h"
#include "list.h"

#define Min(a, b)   (((a) > (b)) ? (b) : (a))
#define Max(a, b)   (((a) > (b)) ? (a) : (b))

#define FS_MAGIC            "LightFS-v1.0"
#define ROOT_MAGIC          "ROOT"
#define HEADER_SCT_IDX      0
#define ROOT_SCT_IDX        1
#define FIXED_SCT_SIZE      2
#define SCT_END_FLAG        ((uint)-1)					//扇区无法使用标记【末尾标记】
#define FE_SIZE				(sizeof(FileEntry))
#define FD_SIZE				(sizeof(FileDesc))
#define FE_ITEM_CNT			(SECT_SIZE / FE_SIZE)		//一个扇区可以存放的FileEntry数量
#define MAP_ITEM_CNT        (SECT_SIZE / sizeof(uint))	//一个扇区可以存放的管理单元的数量

enum
{
    FS_FAILED,	//文件创建失败
    FS_SUCCEED,	//文件创建成功
    FS_EXISTED,	//文件已经存在
    FS_NOEXIST	//文件不存在
};

typedef struct              //存储于0扇区，记录文件系统概要信息
{
    byte forJmp[4];         //预留给jmp指令，0号扇区存储引导程序
    char magic[32];         //存放字符串，显示当前文件系统名称
    uint sctNum;            //当前硬盘扇区数量
    uint mapSize;           //扇区分配表的大小
    uint freeNum;           //维护空闲链表
    uint freeBegin;         //维护空闲链表
} FSHeader;


typedef struct              //存储于1扇区，记录根目录相关信息
{
    char magic[32];         //记录根目录名称

    //记录根目录涉及哪些扇区
    uint sctBegin;          //起始扇区
    uint sctNum;            //占用的扇区总数
    uint lastBytes;         //记录最后一个扇区用了多少字节
} FSRoot;

typedef struct {
    uint *pSct;         //指向对应分配（管理）单元所在的扇区
    uint sctIdx;        //原始数据绝对扇区号
    uint sctOff;        //原始数据绝对扇区号对应的分配单元的位置【扇区偏移】
    uint idxOff;        //原始数据绝对扇区号对应的分配单元的位置【扇区内偏移】
} MapPos;

typedef struct
{
    char name[32];		//文件名
    uint sctBegin;		//文件占用的起始扇区
    uint sctNum;		//文件占用的扇区数
    uint lastBytes;		//文件占用最后一个扇区已使用的字节数
    uint type;			//文件类型
    uint inSctIdx;		//当前FileEntry目录项所在扇区号
    uint inSctOff;		//当前FileEntry目录项所在扇区内偏移
    uint reserved[2];
} FileEntry;

typedef struct
{
    ListNode head;
    FileEntry fe;
    uint objIdx;	//文件读写指针【所在数据扇区偏移】
    uint offset;	//文件读写指针【所在数据扇区内偏移】
    uint changed;	//文件是否有改动
    byte cache[SECT_SIZE];	//文件缓冲区
}FileDesc;

class FSTools {
public:
    FSTools();

    ~FSTools() = default;

    FSTools(const FSTools &obj) = delete;

    FSTools &operator=(const FSTools &obj) = delete;

    FSTools(FSTools &&obj) = delete;

    FSTools &operator=(FSTools &&obj) = delete;

public:
    void FSModInit();

    void SetHDTools(HDTools *hdt);

    uint FSFormat();

    uint FSIsFormatted();

    uint FCreate(const char* fileName);

    uint FExisted(const char* fileName);

    uint FDelete(const char* fileName);

    uint FRename(const char* oldName, const char* newName);

    uint FOpen(const char* fileName);

    void FClose(uint fd);

    int FWrite(uint fd, byte* buff, uint len);

    int FRead(uint fd, byte* buff, uint len);

    int FErase(uint fd, uint bytes);

    int FSeek(uint fd, uint pos);

    int FLength(uint fd);

    uint FTell(uint fd);

    uint FFlush(uint fd);

    //功能测试
    void AllocFreeTest();

    void ForEachFreeSctTest();

    void CreateInRootTest();

    void FindTargetFileTest();

    void FileCreateTest();

    void FileDeleteTest();

    void FileRenameTest();

    void FileOpenWriteTest();

private:
    void *ReadSector(uint si);

    MapPos FindInMap(uint si);

    uint AllocSector();

    uint FreeSector(uint si);

    uint NextSector(uint si);

    uint FindLast(uint sctBegin);

    uint FindPre(uint sctBegin, uint si);

    uint FindIndex(uint sctBegin, uint idx);

    uint MarkSector(uint si);

    uint AddToLast(uint sctBegin, uint si);

    uint CheckStorage(FSRoot* fe);

    uint CreateFileEntry(const char* fileName, uint sctBegin, uint lastBytes);

    uint CreateInRoot(const char* fileName);

    FileEntry* FindInSector(const char* fileName, FileEntry*feBase, uint num);

    FileEntry* FindFileEntry(const char* fileName, uint sctBegin, uint sctNum, uint lastBytes);

    FileEntry* FindInRoot(const char* fileName);

    uint IsOpened(const char* name);

    uint FreeFile(uint sctBegin);

    void MoveFileEntry(FileEntry* dst, FileEntry* src);

    uint AdjustStorage(FSRoot* fe);

    uint EraseLast(FSRoot* fe, uint bytes);

    uint DeleteInRoot(const char* fileName);

    uint FlushFileEntry(FileEntry* fe);

    uint IsFDValid(FileDesc* fd);

    uint FlushCache(FileDesc* fd);

    uint ToFlush(FileDesc* fd);

    uint ReadToCache(FileDesc* fd, uint objIdx);

    uint PrepareCache(FileDesc* fd, uint objIdx);

    int CopyToCache(FileDesc* fd, byte* buff, uint len);

    int ToWrite(FileDesc* fd, byte* buff, uint len);

    uint GetFileLen(FileDesc* fd);

    uint GetFilePos(FileDesc* fd);

    int CopyFromCache(FileDesc* fd, byte* buff, uint len);

    int ToRead(FileDesc* fd, byte* buff, uint len);

    int ToLocate(FileDesc* fd, uint pos);



private:
    HDTools *hd;
    List gFDList;
};

