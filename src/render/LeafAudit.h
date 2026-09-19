#pragma once
#include "core/Types.h"
namespace obvr::render {
struct LeafAuditResult { unsigned leftOnly=0,rightOnly=0,both=0,callsFirst=0,callsSecond=0; bool overflow=false; };
class LeafAudit {
 struct Entry {UInt32 key;unsigned mask;};
 Entry entries[512]{};unsigned count=0,phase=0,calls[2]={};bool overflow=false;
public:
 void Begin() {count=0;phase=1;calls[0]=calls[1]=0;overflow=false;}
 void Second() {phase=2;}
 void Record(UInt32 key) {
  if(!phase || !key) return;
  ++calls[phase-1];
  for(unsigned i=0;i<count;++i) if(entries[i].key==key) {entries[i].mask|=phase;return;}
  if(count==512) {overflow=true;return;}
  entries[count++]={key,phase};
 }
 LeafAuditResult End() {
  phase=0;LeafAuditResult r;r.overflow=overflow;r.callsFirst=calls[0];r.callsSecond=calls[1];
  for(unsigned i=0;i<count;++i) {
   if(entries[i].mask==1) ++r.leftOnly;
   else if(entries[i].mask==2) ++r.rightOnly;
   else ++r.both;
  }
  return r;
 }
};
}
