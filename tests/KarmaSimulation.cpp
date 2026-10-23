// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#include "../src/Karma.h"
#include <iostream>
#include <vector>
struct Stats {
    unsigned successes=0,successRun=0,failureRun=0,maxSuccess=0,maxFailure=0;
    void add(bool success) {
        if(success){++successes;++successRun;failureRun=0;maxSuccess=std::max(maxSuccess,successRun);}
        else{++failureRun;successRun=0;maxFailure=std::max(maxFailure,failureRun);}
    }
};
int main(){
    constexpr std::uint64_t seed=20261019;
    std::cout<<"Seed "<<seed<<"; 500 attempts per fixed chance. Actual Karma.h implementation.\n";
    std::cout<<"chance,mode,successes,failures,longest_success_streak,longest_failure_streak\n";
    for(unsigned chance:{25u,50u,75u,90u}){
        for(bool enabled:{true,false}){
            PO::KarmaRolls rng;rng.seed=seed;Stats stats;
            for(unsigned n=0;n<500;++n)stats.add((enabled?rng.draw():rng.uniform(100))<chance);
            std::cout<<chance<<","<<(enabled?"karma":"independent")<<","<<stats.successes<<","<<500-stats.successes<<","<<stats.maxSuccess<<","<<stats.maxFailure<<"\n";
        }
    }
    // Estimate typical maximum streaks over repeated 500-attempt experiments,
    // so a single seed is not mistaken for evidence of a guaranteed improvement.
    std::cout<<"Average maximum streaks over 1000 separate 500-attempt runs:\n";
    std::cout<<"chance,mode,mean_longest_success_streak,mean_longest_failure_streak\n";
    for(unsigned chance:{25u,50u,75u,90u})for(bool enabled:{true,false}){
        double longestSuccess=0,longestFailure=0;
        for(unsigned trial=0;trial<1000;++trial){PO::KarmaRolls rng;rng.seed=seed+trial;Stats stats;
            for(unsigned n=0;n<500;++n)stats.add((enabled?rng.draw():rng.uniform(100))<chance);
            longestSuccess+=stats.maxSuccess;longestFailure+=stats.maxFailure;
        }
        std::cout<<chance<<","<<(enabled?"karma":"independent")<<","<<longestSuccess/1000<<","<<longestFailure/1000<<"\n";
    }
}
