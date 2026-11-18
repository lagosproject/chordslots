#include "chordslots/Generator.h"
#include <cstdio>
using namespace cs;
enum { MAJ, MIN, DOM, DIM, OTH };
static int cls(int type){ bool h3=false,h4=false,h6=false,h10=false;
  for(int iv: chordTypes()[(size_t)type].iv){ int x=iv%12; if(x==3)h3=true; if(x==4)h4=true; if(x==6)h6=true; if(x==10)h10=true; }
  if(h4) return h10?DOM:MAJ; if(h3) return h6?DIM:MIN; return OTH; }
int main(int argc,char**argv){
  int seeds=atoi(argv[1]); int bySlot=argc>2;
  for(int m=0;m<(int)moods().size();++m){
    int n=0,Vm=0,vmin=0,b7=0,t0=0; int sn[4]={0},sV[4]={0},sv[4]={0},sb[4]={0};
    for(int seed=1;seed<=seeds;++seed){ Progression q; q.tonic=0; q.style=-1; layoutEven(q); applyMood(q,m,true); Rng rng(seed); roll(q,rng);
      int prev=-1,prevT=-1;
      for(int k=0;k<q.numSlots;++k){ auto c=q.slots[k].chord; int d=mod12(c.root); int cl=cls(c.type);
        if(d==prev&&c.type==prevT) continue; prev=d; prevT=c.type;
        ++n; if(k<4)++sn[k]; if(d==0)++t0;
        if(d==7){ if(cl==MAJ||cl==DOM){++Vm; if(k<4)++sV[k];} else if(cl==MIN){++vmin; if(k<4)++sv[k];} }
        if(d==10&&cl==MAJ){++b7; if(k<4)++sb[k];} } }
    printf("%-11s %-14s V%5.1f v%5.1f b7%5.1f t%5.1f",moods()[m].name,scales()[moods()[m].scale].name,100.0*Vm/n,100.0*vmin/n,100.0*b7/n,100.0*t0/n);
    if(bySlot){ printf(" | b7/slot"); for(int k=0;k<4;++k)printf(" %3.0f",100.0*sb[k]/std::max(1,sn[k])); printf(" v/slot"); for(int k=0;k<4;++k)printf(" %3.0f",100.0*sv[k]/std::max(1,sn[k])); }
    printf("\n"); }
}
