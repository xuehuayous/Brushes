//
//  CZFileManager.h
//  BrushesCore
//
//  Created by CharlyZhang on 15/11/24.
//  Copyright © 2015年 CharlyZhang. All rights reserved.
//

#ifndef _CZFILEMANAGER_H_
#define _CZFILEMANAGER_H_

#include <stdio.h>
#include <string>

class CZPainting;

class CZFileManager
{
public:
    static CZFileManager * getInstance()
    {
        static CZFileManager instance;
        return &instance;
    }
    
    void setDirectoryPath(const char* pathStr);
    
    bool savePainting(CZPainting *painting, const char *filenameStr);
    
    CZPainting* createPainting(const char *filenameStr);
    
private:
    CZFileManager();
    ~CZFileManager();
    
private:
    std::string directoryPath;
    
};

#endif /* _CZFILEMANAGER_H_ */
