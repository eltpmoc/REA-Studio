// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/process_memory.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <array>
#include <algorithm>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace rea;
namespace {
void check(bool v,const char* message){if(!v) throw std::runtime_error(message);}
ProcessDescriptor find(uint32_t pid){auto ps=enumerateProcesses();auto p=std::find_if(ps.begin(),ps.end(),[&](const auto& p){return p.pid==pid;});check(p!=ps.end(),"process present in enumeration");return *p;}
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected,"invalid operation must be rejected");}
}
int main(int argc,char** argv){
    if(argc==2 && std::string(argv[1])=="--memory-child"){
        std::array<unsigned char,32> marker{};marker.fill(0x5a);marker[0]=0x41;
        std::cout<<currentProcessId()<<' '<<reinterpret_cast<uintptr_t>(marker.data())<<std::endl;
        std::string command;while(std::getline(std::cin,command)){if(command=="quit") break;if(command=="change"){marker[0]=0x42;std::cout<<"changed"<<std::endl;}}
        return marker[0]==0?1:0;
    }
    QCoreApplication app(argc,argv);
    try{
        const auto self=find(currentProcessId());check(self.identity && !self.name.empty(),"self identity and name");
        auto list=enumerateProcesses();check(std::is_sorted(list.begin(),list.end(),[](const auto& a,const auto& b){return a.pid<b.pid;}),"PID deterministic ordering");
        check(std::adjacent_find(list.begin(),list.end(),[](const auto& a,const auto& b){return a.pid==b.pid;})==list.end(),"unique PIDs");
        ProcessMemory memory(self);std::array<uint8_t,32> marker{};marker.fill(0x7b);marker[0]=0x31;
        const auto address=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(marker.data()));auto regions=memory.regions();
        check(std::any_of(regions.begin(),regions.end(),[&](const auto& r){return r.readable && r.start<=address && address<r.end;}),"stack address mapped readable");
        for(size_t n=0;n<regions.size();++n){check(regions[n].start<regions[n].end,"valid region bounds");if(n)check(regions[n-1].end<=regions[n].start,"non-overlapping ordered map");}
        auto first=memory.read(address,marker.size());check(first.error.empty() && first.bytes==std::vector<uint8_t>(marker.begin(),marker.end()),"real process memory read");
        marker[0]=0x32;auto second=memory.read(address,marker.size());check(second.bytes.front()==0x32,"live read sees changed RAM, not executable file");
        rejects([&]{memory.read(address,0);});rejects([&]{memory.read(address,4097);});rejects([&]{memory.read(UINT64_MAX,2);});
        auto invalid=memory.read(0,32);check(invalid.bytes.empty() && !invalid.error.empty(),"inaccessible bytes are not fabricated zeros");
        auto stale=self;++stale.identity;rejects([&]{ProcessMemory changed(stale);});rejects([]{ProcessMemory missing({UINT32_MAX,1,"missing",{}});});
        std::stop_source cancelled;cancelled.request_stop();rejects([&]{enumerateProcesses(cancelled.get_token());});rejects([&]{memory.regions(cancelled.get_token());});rejects([&]{memory.read(address,32,cancelled.get_token());});
        auto one=std::async(std::launch::async,[&]{ProcessMemory m(self);return m.read(address,32).bytes;});auto two=std::async(std::launch::async,[&]{ProcessMemory m(self);return m.read(address,32).bytes;});check(one.get()==two.get(),"independent concurrent read-only sessions");
        QProcess child;child.setProgram(QCoreApplication::applicationFilePath());child.setArguments({"--memory-child"});child.start();check(child.waitForStarted(5000),"child start");
        check(child.waitForReadyRead(5000),"child address handshake");auto fields=child.readLine().trimmed().split(' ');check(fields.size()==2,"child handshake");
        const auto childPid=fields[0].toUInt();const auto childAddress=fields[1].toULongLong();ProcessMemory remote(find(childPid));
        auto childFirst=remote.read(childAddress,32);check(childFirst.bytes.size()==32 && childFirst.bytes.front()==0x41,"actual child memory");
        child.write("change\n");check(child.waitForBytesWritten(5000) && child.waitForReadyRead(5000),"change child memory");check(child.readLine().trimmed()=="changed","change ack");
        auto childSecond=remote.read(childAddress,32);check(childSecond.bytes.size()==32 && childSecond.bytes.front()==0x42,"child changes visible on refresh");
        rejects([&]{remote.write(childAddress,childSecond.bytes,childFirst.bytes);});
        ProcessMemory writer(find(childPid),true);auto patch=writer.write(childAddress,childSecond.bytes,childFirst.bytes);check(patch.verified && patch.written==32 && remote.read(childAddress,32).bytes==childFirst.bytes,"write child and verified readback");
        auto conflict=writer.write(childAddress,childSecond.bytes,childSecond.bytes);check(!conflict.verified && conflict.written==0 && remote.read(childAddress,32).bytes==childFirst.bytes,"stale expected bytes refused");
        auto undo=writer.write(childAddress,patch.after,patch.before);check(undo.verified && remote.read(childAddress,32).bytes==childSecond.bytes,"conditional restoration");
        rejects([&]{writer.write(childAddress,childSecond.bytes,childFirst.bytes,cancelled.get_token());});rejects([&]{writer.write(childAddress,{},{});});
        auto protectedRegion=std::find_if(regions.begin(),regions.end(),[](const auto& r){return r.readable && !r.writable;});
        if(protectedRegion!=regions.end()){ProcessMemory selfWriter(self,true);auto old=selfWriter.read(protectedRegion->start,1);rejects([&]{selfWriter.write(protectedRegion->start,old.bytes,old.bytes);});}
        child.write("quit\n");child.waitForBytesWritten(5000);check(child.waitForFinished(5000),"child exited");
        auto dead=remote.read(childAddress,32);check(dead.bytes.empty() && !dead.error.empty(),"terminated target is reported");
        std::cout<<"PASS process listing, identities, maps, real self/child reads, live changes, exit, limits, cancellation and concurrency\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
