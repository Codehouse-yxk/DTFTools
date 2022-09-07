#pragma once

#include <iostream>
#include <cstring>
#include "type.h"

#define SECT_SIZE   512

class HDTools final {
public:
    HDTools();

    ~HDTools() = default;

    HDTools(const HDTools &obj) = delete;

    HDTools(HDTools &&obj) = delete;

    HDTools &operator=(const HDTools &obj) = delete;

    HDTools &operator=(HDTools &&obj) = delete;

public:
    void SetFileName(const std::string& name);

    bool ReadFileInfo();

    uint GetHDSectors() const;

    bool WriteData(uint si, byte *buf);

    bool ReadData(uint si, byte *buf);

    bool FlushDataToFile();

private:
    std::string fileName;
    uint sectors;
    byte *gBuff;
};
