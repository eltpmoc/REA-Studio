// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/symbol_names.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>
#ifdef REA_HAS_CXA_DEMANGLE
#include <cxxabi.h>
#endif
#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <mutex>
#endif
namespace rea {
std::string demangleSymbol(std::string_view name) {
    if(name.empty() || name.size()>512 || name.find('\0')!=std::string_view::npos) return {};
    // Limit expansion/recursion opportunities before entering a non-cancellable library.
    if(std::count(name.begin(),name.end(),'S')>12 || std::count(name.begin(),name.end(),'I')>12 || std::count(name.begin(),name.end(),'N')>12) return {};
    const std::string input(name);
#ifdef REA_HAS_CXA_DEMANGLE
    if(name.starts_with("_Z")) {
        int status=-1;std::unique_ptr<char,decltype(&std::free)> result(abi::__cxa_demangle(input.c_str(),nullptr,nullptr,&status),&std::free);
        if(!result || status!=0) return {};
        size_t length=0;while(length<16384 && result.get()[length]) ++length;
        if(length==16384) return {};
        return std::string(result.get(),length);
    }
#endif
#ifdef _WIN32
    if(name.front()=='?') {
        static std::mutex dbghelpMutex;std::lock_guard lock(dbghelpMutex);
        char output[16384]{};
        if(UnDecorateSymbolName(input.c_str(),output,sizeof(output),UNDNAME_COMPLETE)) return output;
    }
#endif
    return {};
}
}
