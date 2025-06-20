#pragma once

#include <QString>

namespace langscore
{
static int wordCountUTF8(QString text)
{
    int words = 0;
    for(auto beg = text.cbegin(); beg != text.cend(); ++beg){
        if((beg->toLatin1() & 0xc0) != 0x80){ words++; }
    }
    return words;
}

static QString getExtension(QString fileName)
{
    int index = fileName.lastIndexOf('.');
    if(index == -1){ return QString(); }
    fileName.remove(0, index+1);
    return fileName;
}


static QString withoutExtension(QString fileName)
{
    int index = fileName.lastIndexOf('.');
    if(index == -1){ return fileName; }

    fileName.remove(index, fileName.length()-index);

    return fileName;
}

static QString withoutAllExtension(QString fileName)
{
    int index = fileName.indexOf('.');
    if(index == -1){ return fileName; }

    fileName.remove(index, fileName.length()-index);

    return fileName;
}


inline QString GetScriptExtension(settings::ProjectType type)
{
    auto scriptExt = ".rb";
    if(type == settings::MV || type == settings::MZ) {
        scriptExt = ".js";
    }
    return scriptExt;
}

}
