// SPDX-License-Identifier: GPL-3.0-or-later
#include "elf_harness.hpp"
#include "analysis_fixtures.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
int main(int argc,char** argv) {
    try {
        rea::test::Scratch scratch;auto path=scratch.directory/"input.elf";
        std::vector<std::vector<uint8_t>> seeds;
        for(bool wide:{false,true}) for(bool little:{false,true}) {
            seeds.push_back(rea::test::elf(1024,wide,little));
            rea::test::check(rea::fuzz::exercise(seeds.back(),path),"valid class/endian seed rejected");
        }
        if(argc==3 && std::string(argv[1])=="--write-corpus") {
            std::filesystem::create_directories(argv[2]);
            for(size_t i=0;i<seeds.size();++i) rea::test::write(std::filesystem::path(argv[2])/("elf-"+std::to_string(i)),seeds[i]);
            return 0;
        }
        if(argc>1) {std::ifstream file(argv[1],std::ios::binary);seeds.emplace_back(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());}
        std::mt19937 random(0x524541);size_t accepted=0,rejected=0,cases=0;
        auto run=[&](const std::vector<uint8_t>& data){++cases;if(rea::fuzz::exercise(data,path)) ++accepted;else ++rejected;};
        // All prefixes of all seeds, including valid headers with truncated tables.
        for(const auto& seed:seeds) for(size_t length=0;length<=seed.size();length+=7)
            run(std::vector<uint8_t>(seed.begin(),seed.begin()+static_cast<std::ptrdiff_t>(length)));
        for(size_t i=0;i<5000;++i) {
            auto data=seeds[random()%seeds.size()];
            switch(i%5) {
            case 0: // Bit mutation, often preserving ELF identification.
                for(unsigned k=0,n=1+random()%12;k<n;++k) data[random()%data.size()]^=static_cast<uint8_t>(1U<<(random()%8));
                break;
            case 1: { // Extreme offsets, counts, links, strides and symbol fields.
                size_t position=16+random()%(data.size()-24);
                std::fill_n(data.begin()+static_cast<std::ptrdiff_t>(position),8,0xff);break;}
            case 2:data.resize(random()%(data.size()+1));break;
            case 3:for(size_t k=16;k<std::min<size_t>(128,data.size());++k) data[k]=static_cast<uint8_t>(random());break;
            default:for(auto& value:data) value=static_cast<uint8_t>(random());break;
            }
            run(data);
        }
        rea::test::check(accepted>0 && rejected>0,"fuzz corpus did not exercise both outcomes");
        std::cout<<"PASS ELF mutation fuzzing: seed=0x524541; "<<cases<<" cases; "<<accepted<<" accepted; "<<rejected<<" rejected; all four class/endian seeds valid\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL FUZZ: "<<e.what()<<'\n';return 1;}
}
