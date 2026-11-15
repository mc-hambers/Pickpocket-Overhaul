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
  auto packed=load(argv[1]),clean=load(argv[2]);check(discover(packed,0x4164e0).empty());check(discover(packed,0x66f320).empty());
  std::uint32_t pe;std::memcpy(&pe,clean.data()+0x3c,4);std::uint16_t opt,n;std::memcpy(&opt,clean.data()+pe+20,2);std::memcpy(&n,clean.data()+pe+6,2);
  overlayExecutable(packed,[&](auto rva,auto out){bool found=false;for(unsigned i=0;i<n;++i){auto h=pe+24+opt+i*40;std::uint32_t crva,raw,size;std::memcpy(&crva,clean.data()+h+12,4);std::memcpy(&size,clean.data()+h+16,4);std::memcpy(&raw,clean.data()+h+20,4);if(crva==rva&&size>=out.size()){std::memcpy(out.data(),clean.data()+raw,out.size());found=true;break;}}check(found);});
  auto chance=discover(packed,0x4164e0),attempt=discover(packed,0x746f40),alarm=discover(packed,0x66fd20);
  check(chance.size()==2&&chance[0].rva==0x74706c&&chance[1].rva==0x8fb563);check(attempt.size()==1&&attempt[0].rva==0x8fc23b);
  auto anchored=chanceSites(packed,0x4164e0);
  check(anchored.size()==2&&anchored[0].rva==0x74706c&&anchored[1].rva==0x8fb563);
  // Reproduce the live failure: a prior plugin redirects one original call.
  // Also test either call, both calls, and swapped plugin destinations.
  for(unsigned mask=0;mask<4;++mask)for(unsigned order=0;order<2;++order){
   auto changed=packed;
   for(unsigned i=0;i<2;++i)if(mask&(1u<<i))put(changed,anchored[i].fileOffset+1,0x3000000+(i^order)*0x1000-anchored[i].rva-5);
   auto fixed=chanceSites(changed,0x4164e0);
   check(fixed.size()==2&&fixed[0].rva==anchored[0].rva&&fixed[1].rva==anchored[1].rva);
   for(unsigned i=0;i<2;++i){
    auto dest=destination(changed,fixed[i].fileOffset,fixed[i].rva);
    check(dest==((mask&(1u<<i))?0x3000000+(i^order)*0x1000:0x4164e0));
    check(contextMatches(packed,fixed[i],std::span<const std::uint8_t>(changed.data()+fixed[i].fileOffset-8,21)));
   }
   if(mask==3){
    auto broken=changed;broken[fixed[0].fileOffset-10]^=1;
    bool rejected=false;try{chanceSites(broken,0x4164e0);}catch(...){rejected=true;}check(rejected);
    // A CALL opcode replacement is not a supported redirected CALL.
    broken=changed;broken[fixed[1].fileOffset]=0xe9;rejected=false;
    try{chanceSites(broken,0x4164e0);}catch(...){rejected=true;}check(rejected);
    // A second matching caller must fail closed rather than choose a random one.
    broken=changed;std::copy_n(changed.begin()+fixed[0].fileOffset-29,61,broken.begin()+0x2000);
    rejected=false;try{chanceSites(broken,0x4164e0);}catch(...){rejected=true;}check(rejected);
   }
  }
  unsigned inside=0;for(auto s:alarm)inside+=s.rva>=0x746f40&&s.rva-0x746f40<0x800;check(inside==1);
  check(karmaSite(packed,0x746f40).rva>0x746f40);check(equippedTransferSite(packed,attempt[0]).rva>attempt[0].rva);
  const std::vector<std::uint32_t> expectedGround{0x1e91a7,0x1e9285,0x27c6f3,0x3638db,0x678859,0x72af4c,0x739c33,0x8f9ed7,0xa2ff24};
  const auto ground=discover(packed,0x66f320);check(ground.size()==expectedGround.size());
  for(unsigned i=0;i<ground.size();++i){check(ground[i].rva==expectedGround[i]);check(destination(packed,ground[i].fileOffset,ground[i].rva)==0x66f320);check(contextMatches(packed,ground[i],std::span<const std::uint8_t>(packed.data()+ground[i].fileOffset-8,21)));}
  std::cout<<"Actual 1.6.1170 fixture: original/redirected caller combinations, ambiguity rejection, chance, attempt, alarm, karma and equipped-transfer validators passed.\n";
 }
 std::cout<<"Passed "<<checks<<" hook image checks\n";
 return 0;
}
int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& e){std::cerr<<"After check "<<checks<<": "<<e.what()<<"\n";return 1;}}
