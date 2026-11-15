#include "../apps/hardware/boundfieldpermissions_p.h"
#include <iostream>
#include <stdexcept>
using namespace GetDAQ;
void Check(bool v){if(!v)throw std::runtime_error("bound permission regression");}
int main(){
 FieldInfo policy;policy.name="initDone";policy.offset=1;policy.width=1;
 for(bool wireReadOnly:{false,true}) {
  FieldInfo wire=policy;wire.readOnly=wireReadOnly;
  HardwareClientInternal::ApplyBoundFieldPolicy(wire,policy,false);Check(!wire.readOnly);
  HardwareClientInternal::ApplyBoundFieldPolicy(wire,policy,true);Check(wire.readOnly);
  policy.readOnly=true;HardwareClientInternal::ApplyBoundFieldPolicy(wire,policy,false);Check(wire.readOnly);policy.readOnly=false;
 }
 for(int mismatch=0;mismatch<3;++mismatch){
  FieldInfo wire=policy;wire.readOnly=true;
  if(mismatch==0)wire.name="other";
  if(mismatch==1)wire.offset=2;
  if(mismatch==2)wire.width=2;
  bool rejected=false;try{HardwareClientInternal::ApplyBoundFieldPolicy(wire,policy,false);}catch(const std::runtime_error&){rejected=true;}
  Check(rejected && wire.readOnly);
 }
 std::cout<<"PASS: bound writable fields, local/register read-only, all layout mismatches rejected\n";
}
