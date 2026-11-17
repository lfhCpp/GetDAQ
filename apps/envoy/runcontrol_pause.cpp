#include "runcontrol.h"

void RunControl::PauseSlot()
{
    
    if (!Available() || !fActive) return;
    if (!AllAt(All(), 5)) { emit LogSignal("Pause requires every ECC in fresh Running state."); return; }
    Begin("Pause", {{All(), "Pause", 6}});
}
void RunControl::ResumeSlot()
{
    
    if (!Available() || !fActive) return;
    if (!AllAt(All(), 6)) { emit LogSignal("Resume requires every ECC in fresh Paused state."); return; }
    Begin("Resume", {{All(), "Resume", 5}});
}
