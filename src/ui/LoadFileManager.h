#pragma once

#include <vector>
#include <QString>
#include "../settings.h"
#include "../csv.hpp"

class LoadFileManager {
public:
    LoadFileManager() = default;
    ~LoadFileManager() = default;

    void loadFile(std::shared_ptr<settings> setting);
private:

    void loadBasicFiles(std::shared_ptr<settings> setting);
    void loadScriptFiles(std::shared_ptr<settings> setting);
    void loadGraphicsFiles(std::shared_ptr<settings> setting);

};