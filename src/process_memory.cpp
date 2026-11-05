// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/process_memory.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#elif defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace rea {
namespace {
constexpr size_t maxProcesses=65536,maxRegions=131072,metadataBudget=32*1024*1024;
void checkpoint(std::stop_token token) {if(token.stop_requested()) throw std::runtime_error("Process inspection cancelled");}
#ifdef _WIN32
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
};
std::string utf8(const wchar_t* text) {
    const int size=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    if(size<=1) return {};
    std::string out(static_cast<size_t>(size),0);WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),size,nullptr,nullptr);out.pop_back();return out;
}
uint64_t identity(HANDLE handle) {
    FILETIME creation{},exit{},kernel{},user{};
    if(!GetProcessTimes(handle,&creation,&exit,&kernel,&user)) return 0;
    return (uint64_t(creation.dwHighDateTime)<<32)|creation.dwLowDateTime;
}
std::string nativeError(const char* operation) {return std::string(operation)+" failed (Windows error "+std::to_string(GetLastError())+"). Process may have exited or access is denied.";}
#elif defined(__linux__)
struct Fd {
    int value{-1};
    explicit Fd(int fd=-1):value(fd){}
    ~Fd(){if(value>=0) ::close(value);}
};
std::string nativeError(const char* operation) {return std::string(operation)+": "+std::strerror(errno)+". Check process lifetime and Linux ptrace/Yama permissions.";}
std::string smallFile(int directory,const char* name,size_t maximum=8192) {
    Fd file(::openat(directory,name,O_RDONLY|O_CLOEXEC));if(file.value<0) throw std::runtime_error(nativeError(name));
    std::string result;std::array<char,4096> buffer{};
    while(result.size()<maximum) {
        const auto count=::read(file.value,buffer.data(),std::min(buffer.size(),maximum-result.size()));
        if(count<0) {if(errno==EINTR) continue;throw std::runtime_error(nativeError(name));}
        if(!count) return result;
        result.append(buffer.data(),static_cast<size_t>(count));
    }
    throw std::runtime_error("Process metadata exceeds limit");
}
uint64_t identity(int directory) {
    const auto stat=smallFile(directory,"stat");const auto end=stat.rfind(')');
    if(end==std::string::npos) throw std::runtime_error("Malformed process stat");
    std::istringstream fields(stat.substr(end+1));std::string value;
    for(int field=3;field<=22;++field) if(!(fields>>value)) throw std::runtime_error("Truncated process stat");
    uint64_t start{};auto [ptr,error]=std::from_chars(value.data(),value.data()+value.size(),start);
    if(error!=std::errc{} || ptr!=value.data()+value.size() || !start) throw std::runtime_error("Invalid process identity");
    return start;
}
std::string executable(int directory) {
    std::array<char,4096> path{};const auto count=::readlinkat(directory,"exe",path.data(),path.size());
    return count>0?std::string(path.data(),static_cast<size_t>(count)):std::string{};
}
#endif
}
struct ProcessMemory::Native {
#ifdef _WIN32
    Handle handle;
#elif defined(__linux__)
    Fd directory, memory;
#endif
};
uint32_t currentProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#elif defined(__linux__)
    return static_cast<uint32_t>(::getpid());
#else
    return 0;
#endif
}
std::vector<ProcessDescriptor> enumerateProcesses(std::stop_token token) {
    checkpoint(token);std::vector<ProcessDescriptor> result;size_t metadataBytes=0;
    auto retain=[&](ProcessDescriptor p){
        const auto bytes=512+p.name.size()+p.path.size();
        if(bytes>metadataBudget-metadataBytes) throw std::runtime_error("Process metadata exceeds 32 MiB limit");
        metadataBytes+=bytes;result.push_back(std::move(p));
    };
#ifdef _WIN32
    Handle snapshot;snapshot.value=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot.value==INVALID_HANDLE_VALUE) throw std::runtime_error(nativeError("Process snapshot"));
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(snapshot.value,&entry)) throw std::runtime_error(nativeError("Process32First"));
    do {
        checkpoint(token);if(result.size()>=maxProcesses) throw std::runtime_error("Process count exceeds limit");
        ProcessDescriptor p;p.pid=entry.th32ProcessID;p.name=utf8(entry.szExeFile);
        Handle handle;handle.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.pid);
        if(handle.value) {
            p.identity=identity(handle.value);std::array<wchar_t,32768> path{};DWORD size=static_cast<DWORD>(path.size());
            if(QueryFullProcessImageNameW(handle.value,0,path.data(),&size)) p.path=utf8(path.data());
        }
        retain(std::move(p));
    } while(Process32NextW(snapshot.value,&entry));
    if(GetLastError()!=ERROR_NO_MORE_FILES) throw std::runtime_error(nativeError("Process32Next"));
#elif defined(__linux__)
    struct Directory {DIR* value{::opendir("/proc")};~Directory(){if(value) ::closedir(value);}} directory;
    if(!directory.value) throw std::runtime_error(nativeError("List /proc"));
    while(true) {
        checkpoint(token);errno=0;auto* entry=::readdir(directory.value);
        if(!entry) {if(errno) throw std::runtime_error(nativeError("Read /proc"));break;}
        const std::string number=entry->d_name;uint32_t pid{};
        auto [ptr,error]=std::from_chars(number.data(),number.data()+number.size(),pid);
        if(error!=std::errc{} || ptr!=number.data()+number.size() || !pid) continue;
        if(result.size()>=maxProcesses) throw std::runtime_error("Process count exceeds limit");
        ProcessDescriptor p;p.pid=pid;p.name="PID "+number;
        Fd proc(::openat(::dirfd(directory.value),entry->d_name,O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        if(proc.value>=0) {
            try {p.identity=identity(proc.value);p.name=smallFile(proc.value,"comm");if(!p.name.empty() && p.name.back()=='\n') p.name.pop_back();p.path=executable(proc.value);}catch(const std::exception&) {}
        }
        retain(std::move(p));
    }
#else
    throw std::runtime_error("Process inspection is implemented for Linux and Windows only");
#endif
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.pid<b.pid;});checkpoint(token);return result;
}
ProcessMemory::ProcessMemory(const ProcessDescriptor& process,bool writable):native_(std::make_unique<Native>()),process_(process),writable_(writable) {
    if(!process.pid || !process.identity) throw std::runtime_error("Process identity unavailable. Refresh the list; the process may be protected or terminated.");
#ifdef _WIN32
    native_->handle.value=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|(writable?(PROCESS_VM_WRITE|PROCESS_VM_OPERATION):0),FALSE,process.pid);
    if(!native_->handle.value) throw std::runtime_error(nativeError("OpenProcess"));
    if(identity(native_->handle.value)!=process.identity) throw std::runtime_error("Process identity changed. Refresh the list.");
#elif defined(__linux__)
    const auto path="/proc/"+std::to_string(process.pid);
    native_->directory.value=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(native_->directory.value<0) throw std::runtime_error(nativeError("Open process"));
    if(identity(native_->directory.value)!=process.identity) throw std::runtime_error("Process identity changed. Refresh the list.");
    // Both descriptors bind the original task/mm, preventing PID reuse from redirecting a read.
    native_->memory.value=::openat(native_->directory.value,"mem",(writable?O_RDWR:O_RDONLY)|O_CLOEXEC);
    if(native_->memory.value<0) throw std::runtime_error(nativeError("Open process memory"));
    if(identity(native_->directory.value)!=process.identity) throw std::runtime_error("Process identity changed during open.");
#else
    throw std::runtime_error("Unsupported host");
#endif
}
ProcessMemory::~ProcessMemory()=default;
std::vector<MemoryRegion> ProcessMemory::regions(std::stop_token token) const {
    checkpoint(token);std::vector<MemoryRegion> result;
#ifdef _WIN32
    uint64_t address=0;MEMORY_BASIC_INFORMATION info{};
    while(VirtualQueryEx(native_->handle.value,reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),&info,sizeof(info))) {
        checkpoint(token);if(result.size()>=maxRegions) throw std::runtime_error("Memory region count exceeds limit");
        const auto start=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info.BaseAddress));
        if(!info.RegionSize || start>UINT64_MAX-info.RegionSize || start+info.RegionSize<=address) throw std::runtime_error("Invalid Windows memory region");
        const auto protection=info.Protect&0xff;const bool accessible=info.State==MEM_COMMIT && !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS));
        const bool read=accessible && (protection==PAGE_READONLY || protection==PAGE_READWRITE || protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY);
        if(info.State==MEM_COMMIT) result.push_back({start,start+info.RegionSize,read,accessible && (protection==PAGE_READWRITE || protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY),accessible && (protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY),info.Type==MEM_IMAGE?"Image":info.Type==MEM_MAPPED?"Mapped":"Private"});
        address=start+info.RegionSize;
    }
    if(GetLastError()!=ERROR_INVALID_PARAMETER) throw std::runtime_error(nativeError("VirtualQueryEx"));
#elif defined(__linux__)
    if(identity(native_->directory.value)!=process_.identity) throw std::runtime_error("Process has terminated or changed");
    Fd maps(::openat(native_->directory.value,"maps",O_RDONLY|O_CLOEXEC));if(maps.value<0) throw std::runtime_error(nativeError("Read process maps"));
    std::string pending;std::array<char,8192> buffer{};size_t mapBytes=0;
    auto line=[&](const std::string& text){
        checkpoint(token);if(result.size()>=maxRegions) throw std::runtime_error("Memory region count exceeds limit");
        std::istringstream input(text);std::string range,permissions,offset,device,inode;
        if(!(input>>range>>permissions>>offset>>device>>inode)) throw std::runtime_error("Malformed memory map");
        auto dash=range.find('-');if(dash==std::string::npos || permissions.size()<4) throw std::runtime_error("Invalid memory map range");
        uint64_t start{},end{};auto a=std::from_chars(range.data(),range.data()+dash,start,16);auto b=std::from_chars(range.data()+dash+1,range.data()+range.size(),end,16);
        if(a.ec!=std::errc{} || a.ptr!=range.data()+dash || b.ec!=std::errc{} || b.ptr!=range.data()+range.size() || end<=start || (!result.empty() && start<result.back().end)) throw std::runtime_error("Invalid memory map ordering");
        std::string name;std::getline(input,name);auto first=name.find_first_not_of(' ');name=first==std::string::npos?"Anonymous":name.substr(first);
        result.push_back({start,end,permissions[0]=='r',permissions[1]=='w',permissions[2]=='x',std::move(name)});
    };
    while(true) {
        checkpoint(token);auto count=::read(maps.value,buffer.data(),buffer.size());
        if(count<0) {if(errno==EINTR) continue;throw std::runtime_error(nativeError("Read process maps"));}
        if(!count) break;
        if(static_cast<size_t>(count)>metadataBudget-mapBytes) throw std::runtime_error("Memory map exceeds 32 MiB limit");
        mapBytes+=static_cast<size_t>(count);
        for(size_t n=0;n<static_cast<size_t>(count);++n) {
            if(buffer[n]=='\n') {line(pending);pending.clear();}else {if(pending.size()>=16384) throw std::runtime_error("Memory map line exceeds limit");pending+=buffer[n];}
        }
    }
    if(!pending.empty()) line(pending);
#else
    throw std::runtime_error("Unsupported host");
#endif
    checkpoint(token);return result;
}
MemoryPage ProcessMemory::read(uint64_t address,size_t count,std::stop_token token) const {
    checkpoint(token);if(!count || count>maxReadBytes || address>UINT64_MAX-count) throw std::runtime_error("Invalid memory read range (1–4096 bytes)");
    MemoryPage page;page.address=address;page.bytes.resize(count);
#ifdef _WIN32
    SIZE_T copied=0;
    if(!ReadProcessMemory(native_->handle.value,reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),page.bytes.data(),count,&copied)) page.error=nativeError("ReadProcessMemory");
    page.bytes.resize(std::min(count,static_cast<size_t>(copied)));
#elif defined(__linux__)
    static_assert(sizeof(off_t)>=8,"Process memory requires 64-bit file offsets");
    if(address>static_cast<uint64_t>(std::numeric_limits<off_t>::max())) throw std::runtime_error("Address exceeds supported process memory offset");
    auto copied=::pread(native_->memory.value,page.bytes.data(),count,static_cast<off_t>(address));
    if(copied<0) {page.bytes.clear();page.error=nativeError("Read process memory");}else page.bytes.resize(static_cast<size_t>(copied));
#else
    page.bytes.clear();page.error="Unsupported host";
#endif
    if(page.bytes.size()!=count && page.error.empty()) page.error="Partial read: mapping changed or process terminated";
    checkpoint(token);return page;
}
MemoryWriteResult ProcessMemory::write(uint64_t address,std::span<const uint8_t> expected,std::span<const uint8_t> replacement,std::stop_token token) const {
    checkpoint(token);
    if(!writable_) throw std::runtime_error("Session is read only; reopen with writing enabled");
    if(expected.empty() || expected.size()!=replacement.size() || expected.size()>maxReadBytes || address>UINT64_MAX-expected.size()) throw std::runtime_error("Patch requires equal original/replacement lengths of 1–4096 bytes");
    MemoryWriteResult result;result.address=address;
    auto map=regions(token);auto region=std::find_if(map.begin(),map.end(),[&](const auto& r){return r.start<=address && address<r.end;});
    if(region==map.end() || !region->readable || !region->writable || replacement.size()>region->end-address) throw std::runtime_error("Patch must stay inside a currently readable/writable region");
    auto current=read(address,expected.size(),token);result.before=std::move(current.bytes);
    if(!current.error.empty() || result.before.size()!=expected.size() || !std::equal(expected.begin(),expected.end(),result.before.begin())) {result.error="Original bytes changed or unreadable; no write performed";return result;}
    checkpoint(token); // Last cancellation point: after the write, always report actual effects.
#ifdef _WIN32
    SIZE_T written=0;
    if(!WriteProcessMemory(native_->handle.value,reinterpret_cast<LPVOID>(static_cast<uintptr_t>(address)),replacement.data(),replacement.size(),&written)) result.error=nativeError("WriteProcessMemory");
    result.written=std::min(replacement.size(),static_cast<size_t>(written));
    if(region->executable && result.written && !FlushInstructionCache(native_->handle.value,reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),result.written)) result.error=nativeError("FlushInstructionCache");
#elif defined(__linux__)
    const auto written=::pwrite(native_->memory.value,replacement.data(),replacement.size(),static_cast<off_t>(address));
    if(written<0) result.error=nativeError("Write process memory");else result.written=static_cast<size_t>(written);
#else
    result.error="Unsupported host";
#endif
    try {auto after=read(address,replacement.size());result.after=std::move(after.bytes);if(!after.error.empty() && result.error.empty()) result.error=after.error;}
    catch(const std::exception& e){if(result.error.empty()) result.error=e.what();}
    result.verified=result.written==replacement.size() && result.after.size()==replacement.size() && std::equal(replacement.begin(),replacement.end(),result.after.begin()) && result.error.empty();
    if(!result.verified && result.error.empty()) result.error="Write partial or read-back differs; target may have changed the bytes";
    return result;
}

}
