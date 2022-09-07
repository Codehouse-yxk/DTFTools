/*
 * @Author: yangxingkun
 * @Date: 2022-08-28 11:15:18
 * @Description: 文件系统
 * @Github: https://github.com/Codehouse-yxk
 */
#include "FSTools.h"


FSTools::FSTools(): hd(nullptr), gFDList{nullptr, nullptr}
{

}

void FSTools::FSModInit()
{
    List_Init(&gFDList);
}

void FSTools::SetHDTools(HDTools* hdt)
{
    hd = hdt;
}

void* FSTools::ReadSector(uint si)
{
    void* ret = nullptr;

    if(si != SCT_END_FLAG)
    {
        ret = malloc(SECT_SIZE);
        if(!(ret && hd->ReadData(si, (byte*)ret)))
        {
            free(ret);
            ret = nullptr;
        }
    }
    return ret;
}

MapPos FSTools::FindInMap(uint si)
{
    MapPos ret = {0};

    auto header = (FSHeader*)((si != SCT_END_FLAG) ? ReadSector(HEADER_SCT_IDX) : nullptr);

    if(header)
    {
        uint offset = si - header->mapSize - FIXED_SCT_SIZE;    //计算原始数据扇区相对分配扇区偏移
        uint sctOff = offset / MAP_ITEM_CNT;                    //计算原始数据扇区对应的分配扇区的偏移【扇区偏移】
        uint idxOff = offset % MAP_ITEM_CNT;                    //计算原始数据扇区对应的分配扇区的偏移【扇区内偏移】
        uint* ps = (uint*)ReadSector(sctOff + FIXED_SCT_SIZE);         //读取对应分配扇区

        if(ps)
        {
            ret.pSct = ps;
            ret.sctIdx = si;
            ret.sctOff = sctOff;
            ret.idxOff = idxOff;
        }
        free(header);
    }

    return ret;
}

uint FSTools::AllocSector()
{
    uint ret = SCT_END_FLAG;

    auto header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    if(header && (header->freeBegin != SCT_END_FLAG))
    {
        MapPos mp = FindInMap(header->freeBegin);       //获取当前空闲链表首个扇区，并查找其对应的分配单元信息
        if(mp.pSct)
        {
            uint* pInt = &mp.pSct[mp.idxOff];
            uint next = *pInt;
            uint flag = 1;
            ret = header->freeBegin;    //将申请到的扇区地址返回

            header->freeBegin = next + FIXED_SCT_SIZE + header->mapSize;    //计算下一个空闲扇区地址起始地址【绝对地址】
            header->freeNum--;          //空闲扇区减1

            *pInt = SCT_END_FLAG;       //标记当前扇区标记为不可用

            flag = hd->WriteData(HEADER_SCT_IDX, (byte*)header);      //将0扇区信息写回硬盘
            flag = flag && hd->WriteData(mp.sctOff + FIXED_SCT_SIZE, (byte*)mp.pSct); //将已修改的分配扇区写回硬盘

            if(!flag)
            {
                ret = SCT_END_FLAG;
            }
            free(mp.pSct);
        }
        free(header);
    }
    return ret;
}

uint FSTools::FreeSector(uint si)
{
    uint ret = SCT_END_FLAG;

    auto header = (FSHeader*)((si != SCT_END_FLAG) ? ReadSector(HEADER_SCT_IDX) : nullptr);

    if(header)
    {
        MapPos mp = FindInMap(si);       //查找目标数据扇区对应的分配单元信息
        if(mp.pSct)
        {
            uint* pInt = &mp.pSct[mp.idxOff];

            *pInt = header->freeBegin - FIXED_SCT_SIZE - header->mapSize;   //将当前扇区回收至管理单元空闲链表头部
            header->freeBegin = si;     //空闲链表头部就是回收的扇区绝对地址
            header->freeNum++;

            ret = hd->WriteData(HEADER_SCT_IDX, (byte*)header)
                  && hd->WriteData(mp.sctOff + FIXED_SCT_SIZE, (byte*)mp.pSct); //将已修改的扇区写回硬盘

            free(mp.pSct);
        }
        free(header);
    }

    return ret;
}

uint FSTools::FSFormat()
{
    auto header = (FSHeader*)malloc(SECT_SIZE);
    auto root = (FSRoot*)malloc(SECT_SIZE);
    auto p = (uint*)malloc(MAP_ITEM_CNT * sizeof(uint));
    uint ret = 0;

    if(header && root && p)
    {
        uint i = 0;
        uint j = 0;
        uint current = 0;

        //①初始化0号扇区【文件系统信息区】，并写回硬盘
        strcpy(header->magic, FS_MAGIC);
        header->sctNum = hd->GetHDSectors();
        header->mapSize = (header->sctNum - FIXED_SCT_SIZE) / 129 + !!((header->sctNum - FIXED_SCT_SIZE) % 129); //向上取整
        header->freeNum = header->sctNum - header->mapSize - FIXED_SCT_SIZE;
        header->freeBegin = FIXED_SCT_SIZE + header->mapSize;
        ret = hd->WriteData(HEADER_SCT_IDX, (byte*)header);       //将0扇区写回硬盘

        //②初始化1号扇区【根目录区】，并写回硬盘
        strcpy(root->magic, ROOT_MAGIC);
        root->sctNum = 0;   //初始化时根目录是空
        root->sctBegin = SCT_END_FLAG;
        root->lastBytes = SECT_SIZE;
        ret = ret && hd->WriteData(ROOT_SCT_IDX, (byte*)root);    //将根目录区写回硬盘

        //③构建扇区分配表中的空闲扇区链表，并写回硬盘
        for(i=0; ret && (i<header->mapSize) && (current<header->freeNum); i++)
        {
            for(j=0; j<MAP_ITEM_CNT; j++)
            {
                uint *pInt = (uint*)&p[j];
                if(current < header->freeNum)
                {
                    *pInt = current + 1;        //当前节点指向下一个节点【将数据区对应的分配单元组织成链表】
                    if(current == header->freeNum - 1)
                    {
                        *pInt = SCT_END_FLAG;   //最后一个映射节点
                    }
                    current++;
                }
                else
                {
                    break;
                }
            }
            ret = hd->WriteData(i+FIXED_SCT_SIZE, (byte*)p);   //将扇区分配区（表）的数据写回硬盘
        }

        free(p);
        free(root);
        free(header);
    }
    return ret;
}

uint FSTools::FSIsFormatted()
{
    uint ret = 0;

    auto header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
    if(header && root)
    {
        ret = (!strcmp(header->magic, FS_MAGIC))
              && (header->sctNum == hd->GetHDSectors())
              && (!strcmp(root->magic, ROOT_MAGIC));

        free(header);
        free(root);
    }
    return ret;
}

uint FSTools::FCreate(const char* fileName)
{
    uint ret = FExisted(fileName);

    if(ret != FS_EXISTED)
    {
        ret = CreateInRoot(fileName) ? FS_SUCCEED : FS_FAILED;
    }

    return ret;
}

uint FSTools::FExisted(const char* fileName)
{
    uint ret = FS_NOEXIST;
    if(fileName)
    {
        FileEntry* fe = FindInRoot(fileName);
        ret = fe ? FS_EXISTED : FS_NOEXIST;
        free(fe);
    }
    return ret;
}

uint FSTools::IsOpened(const char* name)
{
    uint ret = 0;

    return ret;
}

uint FSTools::FreeFile(uint sctBegin)
{
    uint ret = 0;
    uint slider = sctBegin;

    while(slider != SCT_END_FLAG)
    {
        uint next = NextSector(slider);

        ret += FreeSector(slider);

        slider = next;
    }

    return ret;
}

void FSTools::MoveFileEntry(FileEntry* dst, FileEntry* src)
{
    if(dst && src)
    {
        uint inSctIdx = dst->inSctIdx;
        uint inSctOff = dst->inSctOff;

        *dst = *src;

        dst->inSctIdx = inSctIdx;
        dst->inSctOff = inSctOff;
    }
}

uint FSTools::AdjustStorage(FSRoot* fe)
{
    uint ret = 0;

    if(!fe->lastBytes)
    {
        uint last = FindLast(fe->sctBegin);
        uint prev = FindPre(fe->sctBegin, last);

        if(FreeSector(last) && MarkSector(prev))    //释放最后一个扇区，并标记倒数第二个扇区成为最后一个扇区
        {
            fe->sctNum--;
            fe->lastBytes = SECT_SIZE;
            if(!fe->sctNum) //当扇区数为0，重置sctBegin
            {
                fe->sctBegin = SCT_END_FLAG;
            }
            ret = 1;
        }
    }

    return ret;
}

uint FSTools::EraseLast(FSRoot* fe, uint bytes)
{
    uint ret = 0;

    while (fe->sctNum && bytes>0)
    {
        if(bytes < fe->lastBytes)   //需要抹除的字节数小于最后一个扇区存储的字节数
        {
            fe->lastBytes -= bytes;
            ret += bytes;
            bytes = 0;
        }
        else
        {
            bytes -= fe->lastBytes;
            ret += fe->lastBytes;
            fe->lastBytes = 0;

            AdjustStorage(fe);      //调整最后一个扇区
        }
    }

    return ret;
}

uint FSTools::DeleteInRoot(const char* fileName)
{
    uint ret = 0;
    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
    auto fe = (FileEntry*)FindInRoot(fileName);

    if(root && fe)
    {
        uint lastSct = FindLast(root->sctBegin);        //获取保存FileEntry的最后一个扇区
        auto feTarget = (FileEntry*)ReadSector(fe->inSctIdx); //读取目标FileEntry所在的扇区
        auto feLast = (FileEntry*)((lastSct != SCT_END_FLAG) ? ReadSector(lastSct) : nullptr);

        if(feTarget && feLast)
        {
            uint lastOff = root->lastBytes / FE_SIZE - 1;
            FileEntry* lastItem = &feLast[lastOff];             //定位到最后一个扇区中的最后一个FileEntry
            FileEntry* targetItem = &feTarget[fe->inSctOff];    //定位到目标文件FileEntry

            FreeFile(targetItem->sctBegin);
            MoveFileEntry(targetItem, lastItem);
            EraseLast(root, FE_SIZE);

            ret = hd->WriteData(ROOT_SCT_IDX, (byte*)root)
                  && hd->WriteData(fe->inSctIdx, (byte*)feTarget);
        }

        free(feTarget);
        free(feLast);
    }

    free(root);
    free(fe);

    return ret;
}

uint FSTools::FDelete(const char* fileName)
{
    return fileName && !IsOpened(fileName) && (DeleteInRoot(fileName) ? FS_SUCCEED : FS_FAILED);
}

uint FSTools::FlushFileEntry(FileEntry* fe)
{
    uint ret = 0;

    auto feBase = (FileEntry*)ReadSector(fe->inSctIdx);
    auto feInSct = &feBase[fe->inSctOff];

    if(feBase && feInSct)
    {
        *feInSct = *fe;
        ret = hd->WriteData(feInSct->inSctIdx, (byte*)feBase);
    }
    free(feBase);
    return ret;
}

uint FSTools::FRename(const char* oldName, const char* newName)
{
    uint ret = FS_FAILED;

    if(oldName && !IsOpened(oldName) && newName)
    {
        FileEntry* oldFe = FindInRoot(oldName);
        FileEntry* newFe = FindInRoot(newName);

        if(oldFe && !newFe)
        {
            strcpy(oldFe->name, newName);
            if(FlushFileEntry(oldFe))
            {
                ret = FS_SUCCEED;
            }
        }
        free(oldFe);
        free(newFe);
    }

    return ret;
}

uint FSTools::FOpen(const char* fileName)
{
    FileDesc* ret = nullptr;

    if(fileName && !IsOpened(fileName))
    {
        FileEntry* fe = FindInRoot(fileName);
        ret = (FileDesc*)malloc(FD_SIZE);
        if(fe && ret)
        {
            ret->fe = *fe;
            ret->objIdx = SCT_END_FLAG;
            ret->offset = SECT_SIZE;
            ret->changed = 0;
            List_Add(&gFDList, (ListNode*)ret);
        }
        free(fe);
    }

    return (uint)ret;
}

uint FSTools::IsFDValid(FileDesc* fd)
{
    uint ret = 0;
    ListNode* pos = nullptr;

    List_ForEach(&gFDList, pos)
    {
        if(isEqual(fd, pos))
        {
            ret = 1;
            break;
        }
    }

    return ret;
}

uint FSTools::FlushCache(FileDesc* fd)
{
    uint ret = 0;

    if(fd->changed)
    {
        uint sctIdx = FindIndex(fd->fe.sctBegin, fd->objIdx);   //根据文件占用的扇区链表找到当前正在修改的扇区

        if((sctIdx != SCT_END_FLAG) && (ret = hd->WriteData(sctIdx, fd->cache)))
        {
            fd->changed = 0;
        }
    }
    else    //数据没有发生改变，则不需要写入硬盘
    {
        ret = 1;
    }

    return ret;
}

uint FSTools::ToFlush(FileDesc* fd)
{
    return FlushCache(fd) && FlushFileEntry(&fd->fe);
}

void FSTools::FClose(uint fd)
{
    auto pfd = (FileDesc*)fd;

    if(IsFDValid(pfd))      //检测文件描述符是否合法
    {
        ToFlush(pfd);       //将文件缓冲区的数据刷新到硬盘
        List_DelNode((ListNode*)pfd);   //删除该文件描述符
        free(pfd);
    }
}

uint FSTools::ReadToCache(FileDesc* fd, uint objIdx)
{
    uint ret = 0;

    if(objIdx < fd->fe.sctNum)
    {
        uint sctIdx = FindIndex(fd->fe.sctBegin, objIdx);

        ToFlush(fd);        //先将旧缓冲区中的数据刷新到硬盘

        if((sctIdx != SCT_END_FLAG) && (ret = hd->ReadData(sctIdx, fd->cache)))   //将新扇区数据读到缓冲区
        {
            fd->objIdx = objIdx;
            fd->offset = 0;
            fd->changed = 0;
        }
    }
    return ret;
}

uint FSTools::PrepareCache(FileDesc* fd, uint objIdx)
{
    CheckStorage((FSRoot*)&fd->fe);
    return ReadToCache(fd, objIdx);
}

int FSTools::CopyToCache(FileDesc* fd, byte* buff, uint len)
{
    int ret = -1;

    if(fd->objIdx != SCT_END_FLAG)
    {
        uint n = SECT_SIZE - fd->offset;
        byte* p = &fd->cache[fd->offset];
        n = Min(n, len);

        memcpy(p, buff, n);

        fd->offset += n;
        fd->changed = 1;

        //判断是否是最后一个数据扇区，如果数据长度变大，需要更新读写指针偏移
        if(((fd->fe.sctNum - 1) == fd->objIdx) && (fd->fe.lastBytes < fd->offset))
        {
            fd->fe.lastBytes = fd->offset;
        }
        ret = n;
    }

    return ret;
}

int FSTools::ToWrite(FileDesc* fd, byte* buff, uint len)
{
    int ret = 1;
    int i = 0;
    int n = 0;

    while((i<len) && ret)
    {
        byte* p = &buff[i];

        if(fd->offset == SECT_SIZE) //缓冲区已满，需要准备下一个缓冲区
        {
            ret = PrepareCache(fd, fd->objIdx + 1);
        }

        if(ret)
        {
            n = CopyToCache(fd, p, len - i);
            if(n < 0)
            {
                i = -1;
                break;
            }
            i += n;
        }
    }

    ret = i;

    return ret;
}

int FSTools::FWrite(uint fd, byte* buff, uint len)
{
    int ret = -1;

    if(IsFDValid((FileDesc*)fd) && buff)
    {
        ret = ToWrite((FileDesc*)fd, buff, len);
    }

    return ret;
}

uint FSTools::GetFileLen(FileDesc* fd)
{
    uint ret = 0;
    if(fd->fe.sctBegin != SCT_END_FLAG)
    {
        ret = (fd->fe.sctNum - 1) * SECT_SIZE + fd->fe.lastBytes;
    }
    return ret;
}

uint FSTools::GetFilePos(FileDesc* fd)
{
    uint ret = 0;
    if(fd->objIdx != SCT_END_FLAG)
    {
        ret = fd->objIdx * SECT_SIZE + fd->offset;
    }
    return ret;
}

int FSTools::CopyFromCache(FileDesc* fd, byte* buff, uint len)
{
    int ret = -1;

    if(fd->objIdx != SCT_END_FLAG)
    {
        uint n = SECT_SIZE - fd->offset;
        byte* p = &fd->cache[fd->offset];
        n = Min(n, len);

        memcpy(buff, p, n);

        fd->offset += n;

        ret = n;
    }

    return ret;
}

int FSTools::ToRead(FileDesc* fd, byte* buff, uint len)
{
    int ret = -1;
    uint n = GetFileLen(fd) - GetFilePos(fd);    //计算当前有多少数据供读取
    int i = 0;  //记录已经读到的数据量
    len = Min(len, n);

    while((i < len) && ret)
    {
        byte* p = &buff[i];

        if(fd->offset == SECT_SIZE)
        {
            ret = PrepareCache(fd, fd->objIdx + 1);
        }

        if(ret)
        {
            n = CopyFromCache(fd, p, len - i);
            if(n < 0)
            {
                i = -1;
                break;
            }
        }

        i += n;
    }

    ret = i;

    return ret;
}

int FSTools::FRead(uint fd, byte* buff, uint len)
{
    int ret = -1;

    if(IsFDValid((FileDesc*)fd) && buff)
    {
        ret = ToRead((FileDesc*)fd, buff, len);
    }

    return ret;
}

int FSTools::ToLocate(FileDesc* fd, uint pos)
{
    int ret = -1;

    //计算移动后读写指针的位置
    uint len = GetFileLen(fd);
    pos = Min(len, pos);

    //计算读写指针位置与数据链表扇区位置的关系
    uint objIdx = pos / SECT_SIZE;
    uint offset = pos % SECT_SIZE;
    uint sctIdx = FindIndex(fd->fe.sctBegin, objIdx);
    ToFlush(fd);

    //数据发生变动，扇区发生变化，需要读取一次扇区到缓冲区
    if((sctIdx != SCT_END_FLAG) && (hd->ReadData(sctIdx, fd->cache)))
    {
        fd->objIdx = objIdx;
        fd->offset = offset;

        ret = pos;
    }

    return ret;
}

int FSTools::FErase(uint fd, uint bytes)
{
    int ret = -1;
    auto pf = (FileDesc*)fd;

    if(IsFDValid(pf))
    {
        uint pos = GetFilePos(pf);
        uint len = GetFileLen(pf);

        ret = EraseLast((FSRoot*)&pf->fe, bytes);
        len -= ret;     //擦除后的文件长度

        if(ret && (pos > len))  //擦除后的文件长度小于当前读写指针位置，说明需要重定位
        {
            ToLocate(pf, len);
        }
    }

    return ret;
}

int FSTools::FSeek(uint fd, uint pos)
{
    int ret = -1;
    auto pf = (FileDesc*)fd;

    if(IsFDValid(pf))
    {
        ret = ToLocate(pf, pos);
    }

    return ret;
}

int FSTools::FLength(uint fd)
{
    int ret = -1;
    auto pf = (FileDesc*)fd;

    if(IsFDValid(pf))
    {
        ret = GetFileLen(pf);
    }

    return ret;
}

uint FSTools::FTell(uint fd)
{
    uint ret = 0;
    auto pf = (FileDesc*)fd;

    if(IsFDValid(pf))
    {
        ret = GetFilePos(pf);
    }

    return ret;
}

uint FSTools::FFlush(uint fd)
{
    uint ret = 0;
    auto pf = (FileDesc*)fd;

    if(IsFDValid(pf))
    {
        ret = ToFlush(pf);
    }

    return ret;
}

uint FSTools::NextSector(uint si)
{
    uint ret = SCT_END_FLAG;

    auto header = (FSHeader*)((si != SCT_END_FLAG) ? ReadSector(HEADER_SCT_IDX) : nullptr);

    if(header)
    {
        MapPos mp = FindInMap(si);       //查找目标数据扇区对应的分配单元信息
        if(mp.pSct)
        {
            uint* pInt = &mp.pSct[mp.idxOff];

            if(*pInt != SCT_END_FLAG)
            {
                ret = *pInt + header->mapSize + FIXED_SCT_SIZE; //根据分配单元信息的数据计算链表中下一个扇区的绝对位置
            }

            free(mp.pSct);
        }
        free(header);
    }

    return ret;
}

uint FSTools::FindLast(uint sctBegin)
{
    uint ret = SCT_END_FLAG;
    uint next = sctBegin;

    while (next != SCT_END_FLAG)
    {
        ret = next;
        next = NextSector(next);
    }

    return ret;
}

uint FSTools::FindPre(uint sctBegin, uint si)
{
    uint ret = SCT_END_FLAG;
    uint next = sctBegin;

    while ((next != SCT_END_FLAG) && (next != si))
    {
        ret = next;
        next = NextSector(next);
    }

    if(next == SCT_END_FLAG)
    {
        ret = SCT_END_FLAG;
    }

    return ret;
}

uint FSTools::FindIndex(uint sctBegin, uint idx)
{
    uint ret = sctBegin;
    uint i = 0 ;

    while ((i < idx) && (ret != SCT_END_FLAG))
    {
        i++;
        ret = NextSector(ret);
    }

    return ret;
}

uint FSTools::MarkSector(uint si)
{
    uint ret = (si == SCT_END_FLAG) ? 1 : 0;
    MapPos mp = FindInMap(si);
    if(!ret && mp.pSct)
    {
        uint *pInt = &mp.pSct[mp.idxOff];

        *pInt = SCT_END_FLAG;

        ret = hd->WriteData(mp.sctOff + FIXED_SCT_SIZE, (byte*)mp.pSct);

        free(mp.pSct);
    }

    return ret;
}

uint FSTools::AddToLast(uint sctBegin, uint si)
{
    uint ret = 0;
    uint last = FindLast(sctBegin);

    if(last != SCT_END_FLAG)
    {
        MapPos lmp = FindInMap(last);
        MapPos smp = FindInMap(si);

        if(lmp.pSct && smp.pSct)
        {
            if(lmp.sctOff == smp.sctOff)    //两个扇区对应的管理单元处于同一扇区
            {
                uint* pInt = &lmp.pSct[lmp.idxOff];
                *pInt = lmp.sctOff * MAP_ITEM_CNT + smp.idxOff; //将新增的扇区插入链表末尾

                pInt = &lmp.pSct[smp.idxOff];
                *pInt = SCT_END_FLAG;       //尾部扇区写入结束标记

                ret = hd->WriteData(lmp.sctOff + FIXED_SCT_SIZE, (byte*)lmp.pSct);
            }
            else
            {
                uint* pInt = &lmp.pSct[lmp.idxOff];
                *pInt = smp.sctOff * MAP_ITEM_CNT + smp.idxOff; //将新增的扇区插入链表末尾

                pInt = &smp.pSct[smp.idxOff];
                *pInt = SCT_END_FLAG;       //尾部扇区写入结束标记

                ret = hd->WriteData(lmp.sctOff + FIXED_SCT_SIZE, (byte*)lmp.pSct)
                      && hd->WriteData(smp.sctOff + FIXED_SCT_SIZE, (byte*)smp.pSct);
            }
            free(lmp.pSct);
            free(smp.pSct);
        }
    }
    return ret;
}

uint FSTools::CheckStorage(FSRoot* fe)
{
    uint ret = 0;

    if(fe->lastBytes == SECT_SIZE)  //当数据链表最后一个扇区的空间用完时，需要申请新的扇区
    {
        uint si = AllocSector();

        //将申请到的空间插入数据链表的最后
        if(si != SCT_END_FLAG)
        {
            if(fe->sctBegin == SCT_END_FLAG)
            {
                fe->sctBegin = si;
                ret = 1;
            }
            else
            {
                ret = AddToLast(fe->sctBegin, si);
            }
        }

        if(ret)
        {
            fe->sctNum++;
            fe->lastBytes = 0;
        }
        else
        {
            FreeSector(si);
        }
    }
    return ret;
}

uint FSTools::CreateFileEntry(const char* fileName, uint sctBegin, uint lastBytes)
{
    uint ret = 0;
    uint last = FindLast(sctBegin);
    FileEntry* feBase = nullptr;

    if((last != SCT_END_FLAG) && (feBase = (FileEntry*)ReadSector(last)))
    {
        uint offSet = lastBytes / FE_SIZE;
        FileEntry* fe = &feBase[offSet];

        //初始化新的FileEntry
        strcpy(fe->name, fileName);
        fe->type = 0;   //0表示文件类型
        fe->sctBegin = SCT_END_FLAG;
        fe->sctNum = 0;
        fe->inSctIdx = last;
        fe->inSctOff = offSet;
        fe->lastBytes = SECT_SIZE;

        ret = hd->WriteData(last, (byte*)feBase);

        free(feBase);
    }

    return ret;
}

uint FSTools::CreateInRoot(const char* fileName)
{
    uint ret = 0;
    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);

    if(root)
    {
        CheckStorage(root);     //检测是否有足够空间容纳新文件的FileEntry

        if(CreateFileEntry(fileName, root->sctBegin, root->lastBytes))
        {
            root->lastBytes += FE_SIZE;                 //创建FileEntry成功，更新最后一个扇区使用的字节数
            ret = hd->WriteData(ROOT_SCT_IDX, (byte*)root);   //将根目录（1扇区）信息扇区写回硬盘
        }
    }

    return ret;
}


FileEntry* FSTools::FindInSector(const char* fileName, FileEntry*feBase, uint num)
{
    FileEntry* ret = nullptr;
    int i = 0;

    for (i = 0; i < num; i++)
    {
        FileEntry* fe = &feBase[i];
        if(!strcmp(fe->name, fileName))
        {
            ret = (FileEntry*)malloc(FE_SIZE);
            if(ret)
            {
                *ret = *fe;
            }
            break;
        }
    }
    return ret;
}

FileEntry* FSTools::FindFileEntry(const char* fileName, uint sctBegin, uint sctNum, uint lastBytes)
{
    FileEntry* ret = nullptr;
    uint next = sctBegin;
    uint i = 0;

    //①在前sctNum-1个扇区查找
    for(i=0; i<sctNum-1; i++)
    {
        auto feBase = (FileEntry*)ReadSector(next);
        if(feBase)
        {
            ret = FindInSector(fileName, feBase, FE_ITEM_CNT);
            free(feBase);
        }

        if(ret) break;

        next = NextSector(next);    //若没有找到，继续查找下一个扇区
    }

    //②查找最后一个扇区
    if(!ret)
    {
        auto feBase = (FileEntry*)ReadSector(next);
        if(feBase)
        {
            ret = FindInSector(fileName, feBase, lastBytes / FE_SIZE);
            free(feBase);
        }
    }

    return ret;
}

FileEntry* FSTools::FindInRoot(const char* fileName)
{
    FileEntry* ret = nullptr;

    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);

    if(root && root->sctNum)
    {
        ret = FindFileEntry(fileName, root->sctBegin, root->sctNum, root->lastBytes);
        free(root);
    }

    return ret;
}





















/*=======================功能测试=====================*/
void FSTools::AllocFreeTest()
{
    uint a[8] = {0};
    auto header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    printf("free begin = %d\t sectors = %d\n", header->freeBegin, header->freeNum);

    for(int i=0; i<8; i++)
    {
        a[i] = AllocSector();
    }
    header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    printf("free begin = %d\t sectors = %d\n", header->freeBegin, header->freeNum);
    for(int i=0; i<8; i++)
    {
        FreeSector(a[i]);
        printf("a[%d] = %d\n", i, a[i]);
    }
    header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    printf("free begin = %d\t sectors = %d\n", header->freeBegin, header->freeNum);
}

void FSTools::ForEachFreeSctTest()
{
    uint n = 0;
    uint next = 0;
    auto header = (FSHeader*)ReadSector(HEADER_SCT_IDX);
    printf("free begin = %d\t sectors = %d\n", header->freeBegin, header->freeNum);

    next = header->freeBegin;

    while (next != SCT_END_FLAG)
    {
        n++;
        next = NextSector(next);
    }
    printf("free begin = %d\t sectors = %d\n", header->freeBegin, n);
}

void FSTools::CreateInRootTest()
{
    uint r = CreateInRoot("LightOs.txt");
    r = r && CreateInRoot("Test1.txt");
    r = r && CreateInRoot("Test2.txt");
    r = r && CreateInRoot("Test3.txt");

    if(r)
    {
        auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
        auto feBase = (FileEntry*)ReadSector(root->sctBegin);
        printf("sctNum: %d, lastBytes: %d\n", root->sctNum, root->lastBytes);
        printf("root dic file num: %d\n", root->lastBytes / FE_SIZE);


        for(int i=0; i<root->lastBytes/FE_SIZE; i++)
        {
            printf("file name: %s\n", feBase[i].name);
        }
    }
}

void FSTools::FindTargetFileTest()
{
    auto fe = FindInRoot("LightOs.txt");
    if(fe)
    {
        printf("target file: %s, inSctOff:%d\n", fe->name, fe->inSctOff);
        free(fe);
    }
    fe = FindInRoot("Test1.txt");
    if(fe)
    {
        printf("target file: %s, inSctOff:%d\n", fe->name, fe->inSctOff);
        free(fe);
    }
    fe = FindInRoot("Test2.txt");
    if(fe)
    {
        printf("target file: %s, inSctOff:%d\n", fe->name, fe->inSctOff);
        free(fe);
    }
}

void FSTools::FileCreateTest()
{
    printf("format: %d\n", FSFormat());
    printf("isFormat: %d\n", FSIsFormatted());
    printf("create file1: %d\n", FCreate("1.txt"));
    printf("create file2: %d\n", FCreate("2.txt"));
    printf("create file3: %d\n", FCreate("3.txt"));
    printf("create file4: %d\n", FCreate("4.txt"));
    printf("create file5: %d\n", FCreate("5.txt"));
    printf("create file6: %d\n", FCreate("6.txt"));
    printf("create file7: %d\n", FCreate("7.txt"));
    printf("create file8: %d\n", FCreate("8.txt"));
    printf("create file9: %d\n", FCreate("9.txt"));
    printf("create file10: %d\n", FCreate("10.txt"));
    printf("create file11: %d\n", FCreate("11.txt"));
//    printf("file exist: %d\n", FExisted("eee.txt"));
}

void FSTools::FileDeleteTest()
{
    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
    printf("before delete sctNum: %d\n", root->sctNum);
    printf("before delete last bytes: %d\n", root->lastBytes);

    printf("delete file: %d\n", FDelete("3.txt"));
    printf("delete file: %d\n", FDelete("4.txt"));
    printf("delete file: %d\n", FDelete("5.txt"));
    printf("file exist: %d\n", FExisted("3.txt"));
    printf("file exist: %d\n", FExisted("4.txt"));
    printf("file exist: %d\n", FExisted("5.txt"));

    root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
    printf("after delete sctNum: %d\n", root->sctNum);
    printf("after delete last bytes: %d\n", root->lastBytes);

    for(int i=0; i<root->sctNum-1; i++)
    {
        auto fe = (FileEntry*)ReadSector(root->sctBegin + i);
        for(int j=0; j<FE_ITEM_CNT; j++)
        {
            auto item = &fe[j];
            printf("file name: %s\n", item->name);
        }
    }

    auto n = root->lastBytes / FE_SIZE;
    auto fe = (FileEntry*)ReadSector(root->sctBegin + root->sctNum-1);
    for(int i=0; i<n; i++)
    {
        auto item = &fe[i];
        printf("file name: %s\n", item->name);
    }
}

void FSTools::FileRenameTest()
{
    printf("rename file: %d\n", FRename("3.txt", "test.txt"));
    auto root = (FSRoot*)ReadSector(ROOT_SCT_IDX);
    printf("after delete sctNum: %d\n", root->sctNum);
    printf("after delete last bytes: %d\n", root->lastBytes);

    for(int i=0; i<root->sctNum-1; i++)
    {
        auto fe = (FileEntry*)ReadSector(root->sctBegin + i);
        for(int j=0; j<FE_ITEM_CNT; j++)
        {
            auto item = &fe[j];
            printf("file name: %s\n", item->name);
        }
    }

    auto n = root->lastBytes / FE_SIZE;
    auto fe = (FileEntry*)ReadSector(root->sctBegin + root->sctNum-1);
    for(int i=0; i<n; i++)
    {
        auto item = &fe[i];
        printf("file name: %s\n", item->name);
    }
}

void FSTools::FileOpenWriteTest()
{
    const char* name = "test.txt";
    char str[512] = "light os !!!";

    printf("Is formated: %d\n", FSIsFormatted());

    if(FExisted(name) == FS_EXISTED)
    {
//        FDelete(name);
    }
//    printf("create file: %d\n", FCreate(name));

    //写数据到目标文件
    uint fd = FOpen(name);
    printf("open file fd: %d\n", fd);

    printf("write data result: %d\n", FWrite(fd, (byte*)str, sizeof(str)));

    FClose(fd);

    byte buff[SECT_SIZE] = {0};
    fd = FOpen(name);
    printf("open file fd: %d\n", fd);
    printf("pos: %d\n", FTell(fd));
    printf("seek: %d\n", FSeek(fd, 200));
    printf("pos: %d\n", FTell(fd));
    printf("erase: %d\n", FErase(fd, 400));
    printf("file len: %d\n", FLength(fd));
    printf("pos: %d\n", FTell(fd));
    printf("seek: %d\n", FSeek(fd, 0));

    printf("read data len: %d\n", FRead(fd, buff, sizeof(buff)));
    printf("read data: %s\n", buff);
}
