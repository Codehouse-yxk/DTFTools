#include <iostream>

#include "FSTools.h"

int main() {
    HDTools hd;
    FSTools fs;
    hd.SetFileName(R"(..\hd.img)");
    hd.ReadFileInfo();
    fs.SetHDTools(&hd);
    fs.FSModInit();
//    fs.FSFormat();
//    fs.AllocFreeTest();
//    fs.ForEachFreeSctTest();
//    fs.CreateInRootTest();
//    fs.FindTargetFileTest();
//    fs.FileCreateTest();
//    fs.FileDeleteTest();
//    fs.FileRenameTest();
    fs.FileOpenWriteTest();
    printf("flush to hd result: %d", hd.FlushDataToFile());
    return 0;
}
