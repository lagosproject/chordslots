#include "chordslots/Generator.h"
#include <cstdio>
#include <map>
using namespace cs;
int main(){ int m=4; std::map<std::string,int> h; int n=0;
 for(int seed=1;seed<=600;++seed){ Progression q; q.tonic=0; q.style=-1; layoutEven(q); applyMood(q,m,true); Rng rng(seed); roll(q,rng);
  int pr=-1,pt=-1; for(int k=0;k<q.numSlots;++k){ auto c=q.slots[k].chord; if(c.root==pr&&c.type==pt) continue; pr=c.root;pt=c.type; ++n;
   if(mod12(c.root)==10) h[chordName(c,0,q.scale)]++; } }
 int tot=0; for(auto&kv:h){ printf("%s %.1f%%\n",kv.first.c_str(),100.0*kv.second/n); tot+=kv.second; } printf("all root-10: %.1f%% of %d\n",100.0*tot/n,n); }
