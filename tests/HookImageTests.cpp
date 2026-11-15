#include "ChanceSites.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
using namespace PO::Hooks;
static unsigned checks=0;
void check(bool x){++checks;if(!x)throw std::runtime_error("Hook image regression");}
std::vector<std::uint8_t> load(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing fixture");return {std::istreambuf_iterator<char>(f),{}};}
void put(std::vector<std::uint8_t>& v,unsigned p,std::uint32_t x){std::memcpy(v.data()+p,&x,4);}
int run(int argc,char** argv){
 std::vector<std::uint8_t> file(0x600,0),memory(0x2000,0);put(file,0,0x5a4d);put(file,0x3c,0x80);put(file,0x80,0x4550);put(file,0x84,0x00018664);put(file,0x94,0xf0);put(file,0x98,0x20b);put(file,0x98+56,memory.size());
 const auto section=0x188;put(file,section+8,0x200);put(file,section+12,0x1000);put(file,section+16,0x200);put(file,section+20,0x400);put(file,section+36,0x60000020);
 for(auto p:{0x1020,0x1060}){memory[p]=0xe8;put(memory,p+1,0x1100-p-5);}
 check(discover(file,0x1100).empty());unsigned copied=0;
 overlayExecutable(file,[&](auto rva,auto out){++copied;std::memcpy(out.data(),memory.data()+rva,out.size());});
 auto sites=discover(file,0x1100);check(copied==1&&sites.size()==2&&sites[0].rva==0x1020&&sites[1].rva==0x1060);
 const auto untouched=file[0x100];check(untouched==0);
 for(auto s:sites){std::span<const std::uint8_t> live(memory.data()+s.rva-8,21);check(contextMatches(file,s,live));auto bad=std::vector<std::uint8_t>(live.begin(),live.end());bad[0]^=1;check(!contextMatches(file,s,bad));}
 auto bad=file;put(bad,section+20,0xfffffff0);bool failed=false;try{overlayExecutable(bad,[](auto,auto){});}catch(...){failed=true;}check(failed);
 if(argc==3){
  auto packed=load(argv[1]),clean=load(argv[2]);check(discover(packed,0x4164e0).empty());
  std::uint32_t pe;std::memcpy(&pe,clean.data()+0x3c,4);std::uint16_t opt,n;std::memcpy(&opt,clean.data()+pe+20,2);std::memcpy(&n,clean.data()+pe+6,2);
  overlayExecutable(packed,[&](auto rva,auto out){bool found=false;for(unsigned i=0;i<n;++i){auto h=pe+24+opt+i*40;std::uint32_t crva,raw,size;std::memcpy(&crva,clean.data()+h+12,4);std::memcpy(&size,clean.data()+h+16,4);std::memcpy(&raw,clean.data()+h+20,4);if(crva==rva&&size>=out.size()){std::memcpy(out.data(),clean.data()+raw,out.size());found=true;break;}}check(found);});
  auto chance=discover(packed,0x4164e0),attempt=discover(packed,0x746f40),alarm=discover(packed,0x66fd20);
  check(chance.size()==2&&chance[0].rva==0x74706c&&chance[1].rva==0x8fb563);check(attempt.size()==1&&attempt[0].rva==0x8fc23b);
  unsigned inside=0;for(auto s:alarm)inside+=s.rva>=0x746f40&&s.rva-0x746f40<0x800;check(inside==1);
  check(karmaSite(packed,0x746f40).rva>0x746f40);check(equippedTransferSite(packed,attempt[0]).rva>attempt[0].rva);
  std::cout<<"Actual 1.6.1170 fixture: chance, attempt, alarm, karma and equipped transfer validators passed.\n";
 }
 std::cout<<"Passed "<<checks<<" hook image checks\n";
 return 0;
}
int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<"After check "<<checks<<": "<<e.what()<<"\n";return 1;}}
