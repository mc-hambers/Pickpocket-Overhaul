#include "PreviewContextCache.h"
#include "Core.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace PO;
unsigned checks=0;
void check(bool b){++checks;if(!b)throw std::runtime_error("Preview context regression");}
int main(){
 PreviewContextCache<Situation> cache;PreviewContextKey key{1,4,30,77,true,{1,2,3,4,5,6,7,8}};
 check(!cache.find(key));Situation environment;environment.witnessPenalty=12;environment.detected=true;cache.store(key,environment);
 check(cache.find(key)&&cache.find(key)->witnessPenalty==12);
 auto next=key;++next.frame;check(!cache.find(next));next=key;++next.session;check(!cache.find(next));
 next=key;++next.revision;check(!cache.find(next));next=key;++next.victim;check(!cache.find(next));
 next=key;next.detected=false;check(!cache.find(next));
 for(unsigned i=0;i<8;++i){next=key;next.pose[i]+=.01f;check(!cache.find(next));}
 next=key;next.pose[0]=std::numeric_limits<float>::quiet_NaN();cache.store(next,environment);check(!cache.find(next));
 cache.store(key,environment);auto a=*cache.find(key),b=*cache.find(key);a.value=1;a.weight=.1f;b.value=1000;b.weight=20;
 check(cache.find(key)->value==0&&cache.find(key)->weight==0);check(a.value!=b.value&&a.weight!=b.weight);
 cache.clear();check(!cache.find(key));
 // Inventory rebuild model: 100 item previews share one environment, and each
 // retains its own item calculation. A theft refresh invalidates that context.
 unsigned builds=0;float aggregate=0;
 for(unsigned i=0;i<100;++i){if(!cache.find(key)){cache.store(key,environment);++builds;}auto f=*cache.find(key);f.value=i;f.weight=float(i)/10;aggregate+=f.value+f.weight;}
 check(builds==1&&aggregate>0);cache.clear();check(!cache.find(key));
 cache.store(key,environment);next=key;next.revision++;check(!cache.find(next));
 std::cout<<"Passed "<<checks<<" preview cache checks; 100 same-frame item previews built one shared environment.\n";
}
