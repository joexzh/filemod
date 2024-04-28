//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>

namespace filemod {

    template<typename CfgPather>
    class FS {
    private:
        std::filesystem::path _cfg_path = CfgPather();
    public:
    };

}