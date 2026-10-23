// Simulation only: fresh entropy-derived seed per attempt, persistent mode state.
#include "../src/Karma.h"
#include <random>
#include <iostream>
#include <array>
struct Summary {unsigned wins=0,winRun=0,lossRun=0,maxWins=0,maxLosses=0;};
int main(){
    std::random_device entropy;
    std::cout<<"10000 attempts per chance and mode; fresh seed per attempt shared across modes.\n";
    std::cout<<"chance,mode,successes,failures,success_percent,longest_success_streak,longest_failure_streak\n";
    for(unsigned chance:{25u,50u,75u,90u}){
        std::array<std::uint64_t,10000> seeds;
        for(auto& seed:seeds)seed=(std::uint64_t(entropy())<<32)^entropy();
        for(unsigned mode=0;mode<3;++mode){
            PO::KarmaRolls rng;Summary s;unsigned credit=0;
            for(auto seed:seeds){
                rng.seed=seed;
                unsigned effective=chance;
                if(mode==2)effective=std::min(99u,chance+(credit>150?(credit-150)/10:0));
                auto roll=mode==1?rng.draw():rng.uniform(100);
                bool win=roll<effective;
                if(win){++s.wins;++s.winRun;s.lossRun=0;credit=0;}
                else{++s.lossRun;s.winRun=0;credit=std::min(1000u,credit+chance);}
                s.maxWins=std::max(s.maxWins,s.winRun);s.maxLosses=std::max(s.maxLosses,s.lossRun);
            }
            std::cout<<chance<<","<<(mode==0?"Ordinary":mode==1?"Current karma":"Bad-luck only")<<","<<s.wins<<","<<10000-s.wins<<","<<s.wins/100.0<<","<<s.maxWins<<","<<s.maxLosses<<"\n";
        }
    }
}
