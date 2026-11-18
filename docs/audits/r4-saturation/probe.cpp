// R4 saturation probe: raw (unclamped) Brightness over every type x root x scale, neutral register.
#include "chordslots/Metrics.h"
#include <cstdio>
#include <set>
#include <cmath>
using namespace cs;
static float rawX(Chord c,int tonic,int sc,float reg){
  float raw=0.55f*contextQualityBrightness(c,tonic,sc)+0.25f*modalBrightness(c,tonic,sc)+0.20f*reg;
  return (raw-Calibration::brightLo)/(Calibration::brightHi-Calibration::brightLo); }
static float soft(float x,float k){ if(k<=0) return std::min(1.f,std::max(0.f,x));
  return 0.5f+0.5f*std::tanh(k*(2*x-1))/std::tanh(2*k); } // bounded for x in [-0.5,1.5]
int main(){
  for(float k: {0.f,0.4f,0.8f,1.2f}){
    int n=0,sat0=0,sat1=0; std::set<int> d3; float mn=9,mx=-9;
    int tn=numChordTypes(); std::vector<int> t0(tn),t1(tn),tc(tn);
    for(int sc=0;sc<numScales();++sc) for(int r=0;r<12;++r) for(int t=0;t<tn;++t){
      float x=rawX({r,t,0},0,sc,0.5f); float y=soft(x,k); ++n; tc[t]++; mn=std::min(mn,x);mx=std::max(mx,x);
      if(y<=0.001f){++sat0;t0[t]++;} if(y>=0.999f){++sat1;t1[t]++;} d3.insert((int)std::lround(y*1000)); }
    printf("k=%.1f  n=%d  raw range [%.2f,%.2f]  y<=0.001: %.1f%%  y>=0.999: %.1f%%  distinct(3dp)=%zu\n",k,n,mn,mx,100.0*sat0/n,100.0*sat1/n,d3.size());
    if(k==0.f||k==0.8f){ for(int t=0;t<tn;++t) if(t0[t]*10>tc[t]||t1[t]*10>tc[t]) printf("   %-10s at0 %3.0f%% at1 %3.0f%%\n",chordTypes()[t].suffix,100.0*t0[t]/tc[t],100.0*t1[t]/tc[t]); }
  }
}
