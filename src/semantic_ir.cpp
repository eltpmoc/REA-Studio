// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/semantic_ir.hpp"
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <algorithm>
namespace rea {
namespace {
using Op=SemanticValue::Op;
uint64_t mask(unsigned n){return n==64?UINT64_MAX:(uint64_t{1}<<n)-1;}
struct Alias {std::string root;unsigned bits{},shift{};};
Alias alias(const std::string& r){
    static const char* names[][5]={{"rax","eax","ax","al","ah"},{"rbx","ebx","bx","bl","bh"},{"rcx","ecx","cx","cl","ch"},{"rdx","edx","dx","dl","dh"},{"rsi","esi","si","sil",""},{"rdi","edi","di","dil",""},{"rbp","ebp","bp","bpl",""},{"rsp","esp","sp","spl",""}};
    for(auto& row:names)for(unsigned j=0;j<5;++j)if(r==row[j] && !r.empty())return {row[0],j==0?64u:j==1?32u:j==2?16u:8u,j==4?8u:0u};
    for(unsigned n=8;n<16;++n){auto root="r"+std::to_string(n);if(r==root)return {root,64,0};if(r==root+"d")return {root,32,0};if(r==root+"w")return {root,16,0};if(r==root+"b")return {root,8,0};}
    throw std::runtime_error("unsupported register "+r);
}
struct Engine {
    SemanticResult out; AnalysisControl& control;
    std::map<std::string,size_t> regs,initial; std::map<std::string,int64_t> pointers;
    std::map<int64_t,size_t> memory;
    size_t node(Op op,unsigned bits,uint64_t literal=0,size_t a=0,size_t b=0){
        control.work();control.reserveMemory(128);if(out.values.size()>=8192)throw std::runtime_error("semantic node limit");
        out.values.push_back({op,bits,literal,a,b});return out.values.size()-1;
    }
    size_t constant(unsigned bits,uint64_t v){return node(Op::Constant,bits,v&mask(bits));}
    size_t extract(size_t a,unsigned shift,unsigned bits){
        auto v=out.values[a];if(shift==0 && bits==v.bits)return a;
        if(v.op==Op::Constant)return constant(bits,v.literal>>shift);
        if(v.op==Op::Extract)return extract(v.left,unsigned(v.literal)+shift,bits);
        if(v.op==Op::Join){auto low=out.values[v.left].bits;if(shift+bits<=low)return extract(v.left,shift,bits);if(shift>=low)return extract(v.right,shift-low,bits);}
        return node(Op::Extract,bits,shift,a);
    }
    size_t join(size_t lo,size_t hi){
        auto l=out.values[lo],h=out.values[hi];auto bits=l.bits+h.bits;
        if(l.op==Op::Constant && h.op==Op::Constant)return constant(bits,l.literal|(h.literal<<l.bits));
        if(l.op==Op::Extract && h.op==Op::Extract && l.left==h.left && h.literal==l.literal+l.bits)return extract(l.left,unsigned(l.literal),bits);
        return node(Op::Join,bits,0,lo,hi);
    }
    size_t read(const std::string& r){auto a=alias(r);return extract(regs.at(a.root),a.shift,a.bits);}
    void write(const std::string& r,size_t value){auto a=alias(r);if(out.values[value].bits!=a.bits)throw std::runtime_error("width mismatch");pointers.erase(a.root);
        if(a.bits==64)regs[a.root]=value;
        else if(a.bits==32)regs[a.root]=join(value,constant(32,0));
        else {auto old=regs.at(a.root);auto v=value;if(a.shift)v=join(extract(old,0,a.shift),v);regs[a.root]=join(v,extract(old,a.shift+a.bits,64-a.shift-a.bits));}
    }
    int64_t address(const IROperand& o){
        if((o.segment!="ss" && o.segment!="ds") || o.index!="none" || !pointers.contains(o.base))throw std::runtime_error("unproven memory alias/effective address");
        auto base=pointers.at(o.base);if(o.displacement < -1048576 || o.displacement>1048576 || base < -1048576 || base>1048576)throw std::runtime_error("stack offset limit");return base+o.displacement;
    }
    void store(int64_t p,unsigned bits,size_t v){if(bits%8 || !bits || bits>64 || p < -1048576 || p+int64_t(bits/8)>0 || p<pointers.at("rsp")-128)throw std::runtime_error("memory escapes local stack");for(unsigned n=0;n<bits/8;++n)memory[p+n]=extract(v,n*8,8);}
    size_t load(int64_t p,unsigned bits){if(bits%8 || !bits || bits>64)throw std::runtime_error("memory width");size_t result=0;for(unsigned n=0;n<bits/8;++n){if(!memory.contains(p+n))throw std::runtime_error("uninitialized/nonlocal memory");auto byte=memory.at(p+n);result=n?join(result,byte):byte;}return result;}
    size_t read(const IROperand& o,unsigned bits){if(o.kind==IROperandKind::Register)return read(o.reg);if(o.kind==IROperandKind::Immediate)return constant(bits,o.immediate);if(o.kind==IROperandKind::Memory)return load(address(o),o.bits);throw std::runtime_error("operand unsupported");}
    void write(const IROperand& o,size_t v){if(o.kind==IROperandKind::Register)write(o.reg,v);else if(o.kind==IROperandKind::Memory)store(address(o),o.bits,v);else throw std::runtime_error("destination unsupported");}
    size_t binary(Op op,size_t a,size_t b){auto l=out.values[a],r=out.values[b];if(l.bits!=r.bits)throw std::runtime_error("binary width mismatch");
        if(a==b && (op==Op::Xor || op==Op::Sub))return constant(l.bits,0);
        if(r.op==Op::Constant && r.literal==0 && (op==Op::Add || op==Op::Sub || op==Op::Xor || op==Op::Or))return a;
        if(l.op==Op::Constant && r.op==Op::Constant){uint64_t v=op==Op::Add?l.literal+r.literal:op==Op::Sub?l.literal-r.literal:op==Op::Xor?l.literal^r.literal:op==Op::And?l.literal&r.literal:l.literal|r.literal;return constant(l.bits,v);}
        return node(op,l.bits,0,a,b);
    }
    std::string print(size_t id,std::set<unsigned>& args,unsigned depth=0){control.work();if(depth>64)throw std::runtime_error("expression depth limit");auto v=out.values[id];
        if(v.op==Op::Unknown)throw std::runtime_error("return depends on unknown entry register");
        if(v.op==Op::Argument){args.insert(unsigned(v.literal));return "arg"+std::to_string(v.literal);}
        if(v.op==Op::Constant)return std::to_string(v.literal)+"u";
        auto l=print(v.left,args,depth+1);
        if(v.op==Op::Extract)return "((uint"+std::to_string(v.bits)+"_t)(("+l+") >> "+std::to_string(v.literal)+"))";
        auto r=print(v.right,args,depth+1);
        if(l.size()+r.size()>32768)throw std::runtime_error("printed expression limit");
        control.reserveMemory(l.size()+r.size()+256);
        if(v.op==Op::Join)return "((uint64_t)("+l+") | ((uint64_t)("+r+") << "+std::to_string(out.values[v.left].bits)+"))";
        const char* op=v.op==Op::Add?" + ":v.op==Op::Sub?" - ":v.op==Op::Xor?" ^ ":v.op==Op::And?" & ":" | ";return "("+l+op+r+")";
    }
    void run(const IRFunction& f){
        for(auto r:{"rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp","r8","r9","r10","r11","r12","r13","r14","r15"})regs[r]=node(Op::Unknown,64);
        const char* abi[]={"edi","esi","edx","ecx","r8d","r9d"};for(unsigned i=0;i<6;++i){auto root=alias(abi[i]).root;regs[root]=join(node(Op::Argument,32,i),node(Op::Unknown,32));}
        initial=regs;pointers["rsp"]=0;bool returned=false;uint64_t next=f.entry;
        for(const auto& i:f.instructions){control.work();if(returned || i.address!=next || !i.semanticSafe || i.addressBits!=64)throw std::runtime_error("nonlinear flow or unsupported instruction attributes");next=i.address+i.size;
            auto& ops=i.operands;auto m=i.mnemonic;
            if(m=="endbr64" || m=="nop")continue;
            if(m=="ret"){if(!ops.empty() && ops[0].kind==IROperandKind::Immediate)throw std::runtime_error("RET immediate ABI unsupported");if(pointers.at("rsp")!=0)throw std::runtime_error("unbalanced stack at RET");for(auto r:{"rbx","rbp","r12","r13","r14","r15"})if(regs.at(r)!=initial.at(r))throw std::runtime_error("callee-saved register not restored");out.returned=read("eax");returned=true;continue;}
            if(m=="push"){if(ops.empty() || ops[0].bits!=64)throw std::runtime_error("PUSH width unsupported");auto v=read(ops[0],64);pointers["rsp"]-=8;store(pointers.at("rsp"),64,v);continue;}
            if(m=="pop"){if(ops.empty() || ops[0].kind!=IROperandKind::Register || ops[0].bits!=64 || ops[0].reg=="rsp")throw std::runtime_error("POP destination unsupported");auto v=load(pointers.at("rsp"),64);pointers["rsp"]+=8;write(ops[0],v);continue;}
            if(ops.size()<2)throw std::runtime_error("unsupported instruction: "+m);
            const auto& d=ops[0];const auto& s=ops[1];
            if(m=="mov" && d.kind==IROperandKind::Register && s.kind==IROperandKind::Register && d.bits==64 && pointers.contains(s.reg)){auto p=pointers.at(s.reg);write(d,read(s,64));pointers[d.reg]=p;continue;}
            if((m=="add" || m=="sub") && d.reg=="rsp" && d.bits==64 && s.kind==IROperandKind::Immediate){if(s.immediate>1048576)throw std::runtime_error("stack adjustment limit");pointers["rsp"]+=(m=="add"?1:-1)*int64_t(s.immediate);continue;}
            if(m=="lea"){if(d.bits!=64 || s.kind!=IROperandKind::Memory)throw std::runtime_error("LEA form unsupported");auto p=address(s);write(d,node(Op::Unknown,64));pointers[d.reg]=p;continue;}
            if(m=="mov"){write(d,read(s,d.bits));continue;}
            // Flags-only operations are dead only in this branch/call-free integer-return projection.
            if(m=="cmp" || m=="test"){read(d,d.bits);read(s,d.bits);continue;}
            Op op;if(m=="add")op=Op::Add;else if(m=="sub")op=Op::Sub;else if(m=="xor")op=Op::Xor;else if(m=="and")op=Op::And;else if(m=="or")op=Op::Or;else throw std::runtime_error("unsupported instruction: "+m);
            auto a=read(d,d.bits),b=read(s,d.bits);write(d,binary(op,a,b));
        }
        if(!returned)throw std::runtime_error("no proven RET");
        std::set<unsigned> args;auto expression=print(out.returned,args);std::ostringstream code;
        code<<"// Experimental C-like; System V AMD64 hypothesis, confidence: probable\n// uint32_t bitvector return; signedness and source signature unknown.\n// Normal execution only: local stack, integer return, callee-saved registers; flags not ABI outputs.\n// CET entry validation, memory faults and asynchronous stack observers are outside this projection.\nuint32_t sub_"<<std::hex<<f.entry<<"(";
        if(!args.empty())for(unsigned n=0;n<=*args.rbegin();++n){if(n)code<<", ";code<<"uint32_t arg"<<n;}
        code<<") {\n    return "<<expression<<";\n}\n";out.code=code.str();out.recovered=true;
    }
};
}
SemanticResult recoverIntegerFunction(const IRFunction& f,AnalysisControl& control){Engine e{{},control,{},{},{},{}};try{e.run(f);}catch(const AnalysisStopped&){throw;}catch(const std::exception& ex){e.out.reason=ex.what();e.out.code="// C-like unavailable: "+e.out.reason+"\n// Detailed MachineState below preserves unsupported effects.\n";}
    e.out.uses.resize(e.out.values.size());for(size_t n=0;n<e.out.values.size();++n){control.work();auto v=e.out.values[n];if(v.op==Op::Extract)e.out.uses[v.left].push_back(n);else if(v.op>=Op::Join){e.out.uses[v.left].push_back(n);e.out.uses[v.right].push_back(n);}}return std::move(e.out);}
uint64_t SemanticResult::evaluate(std::array<uint64_t,6> args) const {if(!recovered)throw std::runtime_error("unrecovered function");std::vector<uint64_t> data;for(auto v:values){uint64_t x=0;switch(v.op){case Op::Constant:x=v.literal;break;case Op::Argument:x=args.at(v.literal);break;case Op::Unknown:break;case Op::Extract:x=data.at(v.left)>>v.literal;break;case Op::Join:x=data.at(v.left)|(data.at(v.right)<<values.at(v.left).bits);break;case Op::Add:x=data.at(v.left)+data.at(v.right);break;case Op::Sub:x=data.at(v.left)-data.at(v.right);break;case Op::Xor:x=data.at(v.left)^data.at(v.right);break;case Op::And:x=data.at(v.left)&data.at(v.right);break;case Op::Or:x=data.at(v.left)|data.at(v.right);break;}data.push_back(x&mask(v.bits));}return data.at(returned);}
}
