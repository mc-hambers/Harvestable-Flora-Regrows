#pragma once
#include <algorithm>
#include <cmath>
inline int BoundDays(int value){return std::clamp(value,1,30);}
inline double Deadline(double now,int lo,int hi,double roll){
 lo=BoundDays(lo);hi=std::max(lo,BoundDays(hi));
 return now+lo+(hi-lo)*std::clamp(roll,0.0,1.0);
}
