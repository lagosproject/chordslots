#include <algorithm>
#include "chordslots/Generator.h"
#include <cstdio>
using namespace cs;
static int ti(const char*s){ for(int i=0;i<numChordTypes();++i) if(std::string(chordTypes()[i].suffix)==s) return i; return -1; }
int main(){
  struct Q{const char*n;int r;const char*t;}; 
  Q qs[]={{"i",0,"m"},{"iv",5,"m"},{"v",7,"m"},{"V",7,""},{"V7",7,"7"},{"bVII",10,""},{"bVI",8,""},{"bIII",3,""},{"iio",2,"dim"},{"bVII7",10,"7"},{"bII",1,""},{"bII7",1,"maj7"},{"bvii",10,"m"}};
  for(int m: {4,3}){
    Progression p; p.tonic=0; p.style=-1; layoutEven(p); applyMood(p,m,true);
    auto all=buildCandidates(p.tonic,p.scale,p.adventure);
    printf("\n== %s  scale %s adv %.2f  pool %zu\n",moods()[m].name,scales()[p.scale].name,p.adventure,all.size());
    for(int i=0;i<p.numSlots;++i){
      auto cand=slotCandidates(p,i,all);
      std::vector<std::pair<float,const Candidate*>> v; for(auto&c:cand) v.push_back({slotUnaryCost(p,i,c),&c});
      std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.first<b.first;});
      printf(" slot %d targetB %.2f targetT %.2f | best cost %.4f (%s)\n",i,p.slots[i].targetB,p.slots[i].targetT,v[0].first,chordName(v[0].second->chord,p.tonic,p.scale).c_str());
      for(auto&q:qs){ for(size_t k=0;k<v.size();++k){ auto&c=*v[k].second; if(c.chord.root==q.r&&c.chord.type==ti(q.t)){ printf("    %-5s rank %2zu cost %.4f  B %.2f T %.2f\n",q.n,k+1,v[k].first,c.brightness,c.tension); break; } } }
    }
  }
}
