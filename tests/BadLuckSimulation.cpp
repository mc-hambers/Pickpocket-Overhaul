// Simulation proposal only; not part of the plugin.
#include "../src/Karma.h"
#include <iostream>
struct Result {unsigned wins=0,maxWins=0,maxLosses=0,winRun=0,lossRun=0;};
Result simulate(unsigned chance,bool protection,std::uint64_t seed) {
    PO::KarmaRolls rng;rng.seed=seed;Result result;unsigned credit=0;
    for(unsigned n=0;n<500;++n) {
        unsigned effective=chance;
        if(protection&&chance>0&&chance<100)effective=std::min(99u,chance+(credit>150?(credit-150)/10:0));
        const bool win=rng.uniform(100)<effective;
        if(win){++result.wins;++result.winRun;result.lossRun=0;credit=0;}
        else{++result.lossRun;result.winRun=0;if(chance>0&&chance<100)credit=std::min(1000u,credit+chance);}
        result.maxWins=std::max(result.maxWins,result.winRun);result.maxLosses=std::max(result.maxLosses,result.lossRun);
    }
    return result;
}
int main(){
    std::cout<<"Proposal only. Seed 20261019. 500 attempts per run.\n";
    std::cout<<"chance,mode,wins,losses,longest_success_streak,longest_failure_streak\n";
    for(unsigned chance:{25u,50u,75u,90u})for(bool protection:{false,true}){
        auto s=simulate(chance,protection,20261019);
        std::cout<<chance<<","<<(protection?"bad-luck-only":"ordinary")<<","<<s.wins<<","<<500-s.wins<<","<<s.maxWins<<","<<s.maxLosses<<"\n";
    }
    std::cout<<"Average of 1000 runs of 500 attempts:\nchance,mode,success_percent,mean_longest_failure_streak\n";
    for(unsigned chance:{25u,50u,75u,90u})for(bool protection:{false,true}){
        double wins=0,maxLoss=0;
        for(unsigned trial=0;trial<1000;++trial){auto s=simulate(chance,protection,20261019+trial);wins+=s.wins;maxLoss+=s.maxLosses;}
        std::cout<<chance<<","<<(protection?"bad-luck-only":"ordinary")<<","<<wins/5000<<","<<maxLoss/1000<<"\n";
    }
}
