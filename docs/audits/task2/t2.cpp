// Task 2 probe: contextual vs static Tension as generator control.
// Build twice (kUseContextualTension true/false) from header copies; scoring uses BOTH metrics in both builds.
#include "chordslots/Generator.h"
#include <cstdio>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <algorithm>
using namespace cs;
struct Acc { double s=0; long n=0; void add(double v){s+=v;++n;} double m() const {return n? s/n:0;} };
static double pearson(const std::vector<double>&a,const std::vector<double>&b){
  int n=(int)a.size(); double ma=0,mb=0; for(int i=0;i<n;++i){ma+=a[i];mb+=b[i];} ma/=n;mb/=n;
  double sab=0,saa=0,sbb=0; for(int i=0;i<n;++i){sab+=(a[i]-ma)*(b[i]-mb);saa+=(a[i]-ma)*(a[i]-ma);sbb+=(b[i]-mb)*(b[i]-mb);}
  if(saa<1e-9||sbb<1e-9) return 0; return sab/std::sqrt(saa*sbb); }
static double stat(const Progression&p,int i){ const auto&s=p.slots[i]; return voicedTension(s.chord,p.tonic,p.scale,s.voicing.notes.data(),s.voicing.n); }
static double ctx(const Progression&p,int i){ return measuredContextualTension(p,i); }
static std::vector<float> curve(const char*name,int n){
  std::vector<float> t(n); for(int i=0;i<n;++i){ float x= n>1? (float)i/(n-1):0; std::string s=name;
    if(s=="asc") t[i]=0.2f+0.6f*x; else if(s=="desc") t[i]=0.8f-0.6f*x;
    else if(s=="resolve") t[i]= x<0.75f? 0.25f+0.55f*(x/0.75f) : 0.8f-0.55f*((x-0.75f)/0.25f);
    else if(s=="low") t[i]=0.25f; else t[i]=0.75f; } return t; }
int main(int argc,char**argv){
  int seeds=atoi(argv[1]); const char* tag=argv[2];
  const char* curves[]={"asc","desc","resolve","low","high"};
  int scalesToUse[]={0,1};
  for(int sc: scalesToUse) for(int n: {4,8}) for(const char*cv: curves){
    Acc maeS,maeC,corS,corC,peakS,peakC,jumpTgt,jumpS,jumpC,bMae,distinct,tonic,fit,hasNaN;
    auto tgt=curve(cv,n);
    for(int seed=1;seed<=seeds;++seed){
      Progression p; p.tonic=0; p.scale=sc; p.style=-1; p.adventure=0.3f; layoutEven(p); setNumSlots(p,n);
      for(int i=0;i<n;++i){p.slots[i].targetT=tgt[i];p.slots[i].targetB=0.5f;p.slots[i].targetE=0.6f;}
      Rng rng(seed); roll(p,rng);
      std::vector<double> S(n),C(n),T(n); std::set<std::pair<int,int>> u; bool tn=false;
      for(int i=0;i<n;++i){S[i]=stat(p,i);C[i]=ctx(p,i);T[i]=tgt[i];maeS.add(std::fabs(S[i]-T[i]));maeC.add(std::fabs(C[i]-T[i]));
        bMae.add(std::fabs(p.slots[i].brightness-0.5)); u.insert({p.slots[i].chord.root,p.slots[i].chord.type}); if(mod12(p.slots[i].chord.root-p.tonic)==0)tn=true;}
      corS.add(pearson(S,T)); corC.add(pearson(C,T));
      auto am=[&](const std::vector<double>&v){return (int)(std::max_element(v.begin(),v.end())-v.begin());};
      std::string c=cv; if(c=="asc"||c=="desc"||c=="resolve"){ int pt=am(T); if(c=="desc") pt=0; peakS.add(am(S)==pt); peakC.add(am(C)==pt);}
      jumpTgt.add(T[0]-T[n-1]); jumpS.add(S[0]-S[n-1]); jumpC.add(C[0]-C[n-1]);
      distinct.add((double)u.size()/n); tonic.add(tn); fit.add(fitScore(p));
    }
    printf("ROLL,%s,scale%d,n%d,%s,maeS=%.3f,maeC=%.3f,corS=%.3f,corC=%.3f,peakS=%.2f,peakC=%.2f,jumpT=%.3f,jumpS=%.3f,jumpC=%.3f,bMae=%.3f,distinct=%.2f,tonic=%.2f,fit=%.3f\n",
      tag,sc,n,cv,maeS.m(),maeC.m(),corS.m(),corC.m(),peakS.m(),peakC.m(),jumpTgt.m(),jumpS.m(),jumpC.m(),bMae.m(),distinct.m(),tonic.m(),fit.m());
  }
  // Single-slot edit
  for(int sc: scalesToUse) for(int n: {4,8}) for(int where=0;where<2;++where) for(float dT: {+0.35f,-0.30f}){
    Acc okS,okC,dS,dC,chg,nbS,nbC,otherChg,reachS,reachC;
    for(int seed=1;seed<=seeds;++seed){
      Progression p; p.tonic=0; p.scale=sc; p.style=-1; p.adventure=0.3f; layoutEven(p); setNumSlots(p,n);
      for(int i=0;i<n;++i){p.slots[i].targetT=0.45f;p.slots[i].targetB=0.5f;p.slots[i].targetE=0.6f;}
      Rng rng(seed); roll(p,rng);
      int k= where==0? 1 : n-1; int nx=(k+1)%n;
      double s0=stat(p,k),c0=ctx(p,k),ns0=stat(p,nx),nc0=ctx(p,nx); Chord before=p.slots[k].chord; std::vector<Chord> all; for(int i=0;i<n;++i)all.push_back(p.slots[i].chord);
      float tn=std::clamp(0.45f+dT,0.f,1.f); p.slots[k].targetT=tn;
      Rng r2(seed*7919+13); rerollSlots(p,1u<<k,r2);
      double s1=stat(p,k),c1=ctx(p,k);
      okS.add((s1-s0)*dT>0); okC.add((c1-c0)*dT>0); dS.add((s1-s0)*(dT>0?1:-1)); dC.add((c1-c0)*(dT>0?1:-1));
      chg.add(!(before==p.slots[k].chord)); int oc=0; for(int i=0;i<n;++i) if(i!=k && !(all[i]==p.slots[i].chord)) oc=1; otherChg.add(oc);
      nbS.add(std::fabs(stat(p,nx)-ns0)); nbC.add(std::fabs(ctx(p,nx)-nc0));
      reachS.add(std::fabs(s1-tn)); reachC.add(std::fabs(c1-tn));
    }
    printf("EDIT,%s,scale%d,n%d,%s,dT=%+.2f,chordChanged=%.2f,dirOkS=%.2f,dirOkC=%.2f,gainS=%.3f,gainC=%.3f,errS=%.3f,errC=%.3f,nbDeltaS=%.3f,nbDeltaC=%.3f,otherSlotsChanged=%.2f\n",
      tag,sc,n,where==0?"interior":"last",dT,chg.m(),okS.m(),okC.m(),dS.m(),dC.m(),reachS.m(),reachC.m(),nbS.m(),nbC.m(),otherChg.m());
  }
}
