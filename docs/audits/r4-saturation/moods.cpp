// Per mood: share of the formerly saturated types, tonic, distinct chords, and Brightness target error.
#include "chordslots/Generator.h"
#include <cstdio>
#include <cmath>
using namespace cs;
int main(int argc,char**argv){ int seeds=argc>1?atoi(argv[1]):600;
 for(int m=0;m<(int)moods().size();++m){ int n=0,sat=0,t0=0; double err=0; int ne=0;
  for(int seed=1;seed<=seeds;++seed){ Progression q; q.tonic=0; q.style=-1; layoutEven(q); applyMood(q,m,true); Rng rng(seed); roll(q,rng);
   int pr=-1,pt=-1; for(int k=0;k<q.numSlots;++k){ auto c=q.slots[k].chord; err+=std::abs(q.slots[k].brightness-q.slots[k].targetB); ++ne;
    if(c.root==pr&&c.type==pt) continue; pr=c.root;pt=c.type; ++n; std::string s=chordTypes()[c.type].suffix;
    if(s=="dim"||s=="m7b5"||s=="aug"||s=="maj7#11") ++sat; if(mod12(c.root)==0) ++t0; } }
  printf("%-11s sat-types %5.1f%%  tonic %5.1f%%  |B-target| %.3f\n",moods()[m].name,100.0*sat/n,100.0*t0/n,err/ne); } }
