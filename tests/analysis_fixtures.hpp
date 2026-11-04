// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/binary.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
namespace rea::test {
inline void check(bool value,const char* message) {if(!value) throw std::logic_error(message);}
class Scratch {
public:
    std::filesystem::path directory;
    Scratch() {
        auto base=std::filesystem::temp_directory_path();
        const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<100;++i) {
            auto candidate=base/("rea-analysis-"+std::to_string(tick)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {directory=std::move(candidate);return;}
        }
        throw std::runtime_error("Cannot create fixture directory");
    }
    ~Scratch() {std::error_code error;std::filesystem::remove_all(directory,error);}
    Scratch(const Scratch&)=delete;
    Scratch& operator=(const Scratch&)=delete;
};
inline void write(const std::filesystem::path& path,const std::vector<uint8_t>& bytes) {
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(out),"fixture write failed");
}
inline std::vector<uint8_t> elf(size_t size=1024,bool wide=true,bool little=true) {
    std::vector<uint8_t> data(std::max<size_t>(size,512),0x90);
    std::fill(data.begin(),data.begin()+512,0);
    auto put=[&](size_t offset,uint64_t value,size_t width){
        for(size_t i=0;i<width;++i) data.at(offset+i)=static_cast<uint8_t>(value>>(8*(little ? i : width-i-1)));
    };
    data[0]=0x7f;data[1]='E';data[2]='L';data[3]='F';data[4]=wide ? 2 : 1;data[5]=little ? 1 : 2;data[6]=1;
    put(16,2,2);put(18,wide ? 62 : 3,2);put(20,1,4);put(24,0x400200,wide ? 8 : 4);
    put(wide ? 32 : 28,64,wide ? 8 : 4);
    put(wide ? 52 : 40,wide ? 64 : 52,2);put(wide ? 54 : 42,wide ? 56 : 32,2);put(wide ? 56 : 44,1,2);
    put(64,1,4);put(wide ? 68 : 88,5,4);put(wide ? 80 : 72,0x400000,wide ? 8 : 4);
    put(wide ? 96 : 80,data.size(),wide ? 8 : 4);put(wide ? 104 : 84,data.size(),wide ? 8 : 4);
    data.back()=0xc3;return data;
}
}
