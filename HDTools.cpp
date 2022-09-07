/*
 * @Author: yangxingkun
 * @Date: 2022-08-24 23:11:15
 * @Description: 硬盘文件读写数据功能
 * @Github: https://github.com/Codehouse-yxk
 */

#include "HDTools.h"
#include <fstream>


HDTools::HDTools() : sectors(0), gBuff(nullptr)
{

}

void HDTools::SetFileName(const std::string& name)
{
    if(!name.empty())
    {
        fileName = name;
    }
    else
    {
        std::cout << "name is empty !" << std::endl;
    }
}

bool HDTools::ReadFileInfo()
{
    bool ret = false;
    std::ifstream file;
    file.open(fileName, std::ios::in | std::ios::binary);
    if((ret = file.is_open()))
    {
        int64_t length = 0;
        file.seekg(0, std::ios::end);
        length = file.tellg();
        file.seekg(0, std::ios::beg);
        sectors = length / SECT_SIZE;
        gBuff = (byte*)malloc(length);
        file.read(reinterpret_cast<char*>(gBuff), length);
        file.close();
    }
    else
    {
        std::cout << "file open fail !" << std::endl;
    }
    return ret;
}

uint HDTools::GetHDSectors() const
{
    return sectors;
}

bool HDTools::WriteData(uint si, byte *buf)
{
    bool ret = false;

    if( (ret = ((si < GetHDSectors()) && buf)) )
    {
        byte* p = &gBuff[si * SECT_SIZE];
        memmove(p, buf, SECT_SIZE);
    }

    return ret;
}

bool HDTools::ReadData(uint si, byte *buf)
{
    bool ret = false;

    if( (ret = ((si < GetHDSectors()) && buf)) )
    {
        byte* p = &gBuff[si * SECT_SIZE];

        memmove(buf, p, SECT_SIZE);
    }

    return ret;
}

bool HDTools::FlushDataToFile()
{
    bool ret = false;
    std::ofstream file;
    file.open(fileName, std::ios::out | std::ios::binary);
    if((ret = file.is_open()))
    {
        file.write(reinterpret_cast<char*>(gBuff), GetHDSectors() * SECT_SIZE);
        file.close();
    }
    else
    {
        std::cout << "file open fail !" << std::endl;
    }
    return ret;
}
