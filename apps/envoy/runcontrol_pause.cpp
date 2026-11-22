#include "runcontrol.h"

void RunControl::PauseSlot()
{
    
    if (!Available()) return;
    if (!AllAt(All(), 5)) { emit LogSignal("Pause requires every ECC in fresh Running state."); return; }
    if(!fActive){fJournal=QJsonObject{{"untracked",true}};emit LogSignal("Pausing the observed external acquisition; no local Run records are created.");}
    Begin("Pause", {{All(), "Pause", 6}});
}
void RunControl::ResumeSlot()
{
    
    if (!Available()) return;
    if (!AllAt(All(), 6)) { emit LogSignal("Resume requires every ECC in fresh Paused state."); return; }
    if(!fActive){fJournal=QJsonObject{{"untracked",true}};emit LogSignal("Resuming the observed external acquisition; no new Run or local records are created.");}
    Begin("Resume", {{All(), "Resume", 5}});
}
