#pragma once
#include "TimerRules.h"
inline bool IsNirnBase(unsigned id){return id==0x000E1FB2 || id==0x000B6FB9;}
inline int NirnKindForBase(unsigned id){return id==0x000E1FB2 ? 1 : id==0x000B6FB9 ? 2 : 0;}
inline double FeatureDeadline(double now,int minimum,int maximum,bool custom,int days,double roll){return custom ? now+BoundDays(days) : Deadline(now,minimum,maximum,roll);}
inline bool MilkDue(bool legacy,double now,double deadline){return legacy || now>=deadline;}
