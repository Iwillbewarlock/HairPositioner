Scriptname RaceMenuHairPositioner extends RaceMenuBase
{RaceMenu plugin for HairPositioner. Adds the hair offset sliders to the
 Hair category for the player. Slider layout is table driven: one entry per
 native channel, plus pivot and reset.}

; ---- slider table ---------------------------------------------------------
; index 0..8  -> native channel (same index)
; index 9     -> pivot mode
; index 10    -> reset trigger
int Property SLIDER_COUNT = 11 AutoReadOnly
int Property IDX_PIVOT = 9 AutoReadOnly
int Property IDX_RESET = 10 AutoReadOnly

; ---- persistence through RaceMenu itself ----------------------------------
; Every value is mirrored into RaceMenu's body-morph store under one key.
; RaceMenu writes that store into its character presets (.jslot) and its own
; co-save, and restores it on preset load -- so the hair offset rides along
; with the normal RaceMenu Save/Load Preset buttons, no hooks needed. The
; morph names match no mesh, so RaceMenu never applies them to a body.
string Property MORPH_KEY = "HairPositioner" AutoReadOnly
string Property MORPH_PRESENT = "HPOS_Present" AutoReadOnly
string Property MORPH_PIVOT = "HPOS_Pivot" AutoReadOnly

string[] _label      ; translation key
float[]  _min
float[]  _max
float[]  _step
float[]  _value
string   _callbackPrefix = "HPOS_"

bool _ready = false

Event OnInit()
	Parent.OnInit()
	OnGameReload()
EndEvent

Event OnStartup()
	Parent.OnStartup()
	_ready = SKSE.GetPluginVersion("HairPositioner") >= 0
EndEvent

Function BuildTable()
	_label = new string[11]
	_min   = new float[11]
	_max   = new float[11]
	_step  = new float[11]
	_value = new float[11]

	; move
	SetRow(0, "$HPOS_MoveSide",    -15.0, 15.0, 0.01)
	SetRow(1, "$HPOS_MoveDepth",   -15.0, 15.0, 0.01)
	SetRow(2, "$HPOS_MoveHeight",  -15.0, 15.0, 0.01)
	; rotate
	SetRow(3, "$HPOS_Tilt",        -90.0, 90.0, 0.1)
	SetRow(4, "$HPOS_Lean",        -90.0, 90.0, 0.1)
	SetRow(5, "$HPOS_Turn",        -90.0, 90.0, 0.1)
	; scale
	SetRow(6, "$HPOS_Width",         0.5,  2.0, 0.01)
	SetRow(7, "$HPOS_Depth",         0.5,  2.0, 0.01)
	SetRow(8, "$HPOS_Height",        0.5,  2.0, 0.01)
	; controls
	SetRow(IDX_PIVOT,       "$HPOS_Pivot",      0.0, (HairPositioner.GetPivotCount() - 1) as float, 1.0)
	SetRow(IDX_RESET,       "$HPOS_Reset",      0.0, 1.0, 1.0)
EndFunction

; ---- RaceMenu store <-> plugin -------------------------------------------
string Function MorphNameOf(int channel)
	return "HPOS_C" + channel
EndFunction

; Write one channel (or the pivot) into RaceMenu's store.
Function StoreChannel(int channel)
	Actor player = Game.GetPlayer()
	NiOverride.SetBodyMorph(player, MorphNameOf(channel), MORPH_KEY, HairPositioner.GetChannel(channel))
	NiOverride.SetBodyMorph(player, MORPH_PRESENT, MORPH_KEY, 1.0)
EndFunction

Function StorePivot()
	Actor player = Game.GetPlayer()
	NiOverride.SetBodyMorph(player, MORPH_PIVOT, MORPH_KEY, HairPositioner.GetPivot() as float)
	NiOverride.SetBodyMorph(player, MORPH_PRESENT, MORPH_KEY, 1.0)
EndFunction

; Plugin -> RaceMenu store (everything).
Function StoreAll()
	int c = 0
	while c < 9
		StoreChannel(c)
		c += 1
	endwhile
	StorePivot()
EndFunction

; RaceMenu store -> plugin. Called after RaceMenu (re)initialises the menu,
; which is also what happens right after a character preset is loaded.
Function RestoreAll()
	Actor player = Game.GetPlayer()
	int c = 0
	while c < 9
		float v = NiOverride.GetBodyMorph(player, MorphNameOf(c), MORPH_KEY)
		if c >= 6 && v <= 0.0
			v = 1.0  ; a missing scale entry means "unchanged", never "zero"
		endif
		HairPositioner.SetChannel(c, v)
		c += 1
	endwhile
	HairPositioner.SetPivot(NiOverride.GetBodyMorph(player, MORPH_PIVOT, MORPH_KEY) as int)
EndFunction

; If RaceMenu holds our values (a preset with hair data was loaded, or we
; stored them earlier), they are the truth; otherwise seed the store from
; the plugin so the next preset save picks them up.
Function SyncWithRaceMenu()
	if NiOverride.HasBodyMorph(Game.GetPlayer(), MORPH_PRESENT, MORPH_KEY)
		RestoreAll()
	else
		StoreAll()
	endif
EndFunction

Function SetRow(int i, string label, float lo, float hi, float step)
	_label[i] = label
	_min[i] = lo
	_max[i] = hi
	_step[i] = step
	_value[i] = 0.0
EndFunction

string Function CallbackOf(int i)
	return _callbackPrefix + i
EndFunction

int Function IndexOf(string callback)
	int i = 0
	while i < SLIDER_COUNT
		if callback == CallbackOf(i)
			return i
		endif
		i += 1
	endwhile
	return -1
EndFunction

; Current value of a row as the plugin sees it.
float Function ReadRow(int i)
	if i < 9
		return HairPositioner.GetChannel(i)
	elseif i == IDX_PIVOT
		return HairPositioner.GetPivot() as float
	endif
	return 0.0
EndFunction

; Pull every row from the plugin and push changed ones to the menu.
Function RefreshSliders()
	int i = 0
	while i < SLIDER_COUNT
		float v = ReadRow(i)
		if _value[i] != v
			_value[i] = v
			SetSliderParameters(CallbackOf(i), _min[i], _max[i], _step[i], v)
		endif
		i += 1
	endwhile
EndFunction

Event OnSliderRequest(Actor player, ActorBase playerBase, Race actorRace, bool isFemale)
	if !_ready || player != Game.GetPlayer()
		return
	endif
	BuildTable()
	SyncWithRaceMenu()
	int i = 0
	while i < SLIDER_COUNT
		_value[i] = ReadRow(i)
		AddSlider(_label[i], CATEGORY_HAIR, CallbackOf(i), _min[i], _max[i], _step[i], _value[i])
		i += 1
	endwhile
EndEvent

Event OnSliderChanged(string callback, float value)
	if !_ready || !_label
		return
	endif
	int i = IndexOf(callback)
	if i < 0
		return
	endif
	_value[i] = value

	if i < 9
		HairPositioner.SetChannel(i, value)
		StoreChannel(i)
	elseif i == IDX_PIVOT
		HairPositioner.SetPivot(value as int)
		StorePivot()
	elseif i == IDX_RESET
		if value >= 0.5
			HairPositioner.Reset()
			StoreAll()
			RefreshSliders()
			ClearTrigger(i)
		endif
	endif
EndEvent

; Trigger-style rows (reset) snap back to 0 after firing.
Function ClearTrigger(int i)
	_value[i] = 0.0
	SetSliderParameters(CallbackOf(i), _min[i], _max[i], _step[i], 0.0)
EndFunction
