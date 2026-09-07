Scriptname HairPositioner Hidden
{Native bridge to HairPositioner.dll. Player hair only.

 channel : 0 move left/right   1 move back/forward   2 move down/up
           3 tilt (pitch)      4 lean (roll)          5 turn (yaw)     -- degrees
           6 width             7 depth                8 height         -- scale, 1.0 = unchanged
 pivot   : 0 head bone (default)   1 hair center   2 model origin
 worn    : also move items worn in the wig slots (ini WigSlots, default 31/41).
           OFF by default -- helmets and hoods use those slots too.

 Console (vanilla cgf):
   cgf "HairPositioner.SetChannel" 2 5.0      ; hair up 5 units
   cgf "HairPositioner.Probe"}

bool Function IsReady() global native
bool Function HasHair() global native
string Function GetKey() global native

int Function GetChannelCount() global native
Function SetChannel(int channel, float value) global native
float Function GetChannel(int channel) global native

int Function GetPivotCount() global native
Function SetPivot(int mode) global native
int Function GetPivot() global native

Function SetFollowWorn(bool on) global native
bool Function GetFollowWorn() global native

Function Reset() global native

; Debug: dump what the plugin sees to the console and HairPositioner.log.
Function Probe() global native
Function Show() global native
