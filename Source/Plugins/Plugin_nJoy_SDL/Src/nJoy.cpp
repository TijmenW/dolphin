//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
// Name: nJoy
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

 

////////////////////////////////////////////////////////////////////////////////////////
// Issues
/* ŻŻŻŻŻŻŻŻŻ

   The StrangeHack in ConfigAdvanced.cpp doesn't work in Linux, it still wont resize the
   window correctly. So currently in Linux you have to have advanced controls enabled when
   you open the window to see them.

////////////////////////*/

 

////////////////////////////////////////////////////////////////////////////////////////
// Variables guide
/* ŻŻŻŻŻŻŻŻŻ

   Joyinfo[1, 2, 3, 4, ..., number of attached devices]: Gamepad info that is populate by Search_Devices()
   PadMapping[1, 2, 3 and 4]: The button mapping
   Joystate[1, 2, 3 and 4]: The current button states

   The arrays PadMapping[] and PadState[] are numbered 0 to 3 for the four different virtual
   controllers. Joysticks[].ID will have the number of the physical input device mapped to that
   controller (this value range between 0 and the total number of connected physical devices). The
   mapping of a certain physical device to PadState[].joy is initially done by Initialize(), but
   for the configuration we can remap that, like in ConfigBox::ChangeJoystick().

   The joyinfo[] array holds the physical gamepad info for a certain physical device. It's therefore
   used as joyinfo[PadMapping[controller].ID] if we want to get the joyinfo for a certain joystick.

////////////////////////*/

 

//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ŻŻŻŻŻŻŻŻŻ
#include "nJoy.h"

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	ConfigBox* m_frame;
#endif
/////////////////////////

 
//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ŻŻŻŻŻŻŻŻŻ

// Rumble in windows
#define _CONTROLLER_STATE_H // Avoid certain declarations in nJoy.h
FILE *pFile;
HINSTANCE nJoy_hInst = NULL;
std::vector<CONTROLLER_INFO> joyinfo;
CONTROLLER_STATE PadState[4];
CONTROLLER_MAPPING PadMapping[4];
bool emulator_running = false;
int NumPads = 0, NumGoodPads = 0;
HWND m_hWnd; // Handle to window
SPADInitialize *g_PADInitialize = NULL;

// TODO: fix this dirty hack to stop missing symbols
void __Log(int log, const char *format, ...) {}
void __Logv(int log, int v, const char *format, ...) {}

// Rumble
#ifdef _WIN32

#elif defined(__linux__)
	extern int fd;
#endif

 
//////////////////////////////////////////////////////////////////////////////////////////
// wxWidgets
// ŻŻŻŻŻŻŻŻŻ
#if defined(HAVE_WX) && HAVE_WX
	class wxDLLApp : public wxApp
	{
		bool OnInit()
		{
			return true;
		}
	};

	IMPLEMENT_APP_NO_MAIN(wxDLLApp)
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

 
//////////////////////////////////////////////////////////////////////////////////////////
// DllMain
// ŻŻŻŻŻŻŻ
#ifdef _WIN32
BOOL APIENTRY DllMain(	HINSTANCE hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
		{       
			//use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);

			if (!wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break;

		case DLL_PROCESS_DETACH:		
			wxEntryCleanup(); //use wxUninitialize() if you don't want GUI
		break;

		default:
			break;
	}

	nJoy_hInst = hinstDLL;
	return TRUE;
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// Input Plugin Functions (from spec's)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ

// Get properties of plugin
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (DebugFast) by Falcon4ever");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION " by Falcon4ever");
#else
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (Debug) by Falcon4ever");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals) {}

// Call config dialog
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void DllConfig(HWND _hParent)
{
	// Debugging
	//Console::Open();

	#ifdef _WIN32
		// Start the pads so we can use them in the configuration and advanced controls
		if(!emulator_running)
		{
			NumPads = Search_Devices(); // Populate joyinfo for all attached devices
		}

		m_frame = new ConfigBox(NULL);
		m_frame->ShowModal();

		/* Check if any of the pads failed to open. In Windows there is a strange "IDirectInputDevice2::
		   SetDataFormat() DirectX error -2147024809" after a few Open and Close */
		if (   (PadMapping[0].enabled && PadState[0].joy == NULL)
			|| (PadMapping[1].enabled && PadState[1].joy == NULL)
			|| (PadMapping[2].enabled && PadState[2].joy == NULL)
			|| (PadMapping[3].enabled && PadState[3].joy == NULL))
		{
			//PostMessage(_hParent, 25, 0, 0);
			Console::Print("%s\n", SDL_GetError());		
		}
	#else
		if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
		{
			printf("Could not initialize SDL! (%s)\n", SDL_GetError());
			return;
		}

		g_Config.Load();	// load settings

		#if defined(HAVE_WX) && HAVE_WX
			ConfigBox frame(NULL);
			frame.ShowModal();
		#endif
	#endif
}

void DllDebugger(HWND _hParent, bool Show) {}

 
// Init PAD (start emulation)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
/* Information: This function can not be run twice without a Shutdown in between. If
   it's run twice the SDL_Init() will cause a crash. One solution to this is to keep a
   global function that remembers the SDL_Init() and SDL_Quit() (emulator_running does
   not do that since we can open and close this without any game running). But I would
   suggest that avoiding to run this twice from the Core is better. */
void Initialize(void *init)
{
	// Debugging
	//Console::Open();
	Console::Print("Initialize: %i\n", SDL_WasInit(0));
    g_PADInitialize = (SPADInitialize*)init;
	emulator_running = true;

	#ifdef _DEBUG
		DEBUG_INIT();
	#endif

	#ifdef _WIN32
		m_hWnd = (HWND)g_PADInitialize->hWnd;
	#endif

	NumPads = Search_Devices(); // Populate joyinfo for all attached devices

	/* Check if any of the pads failed to open. In Windows there is a strange "IDirectInputDevice2::
	   SetDataFormat() DirectX error -2147024809" after a few Open and Close */
	if (   (PadMapping[0].enabled && PadState[0].joy == NULL)
		|| (PadMapping[1].enabled && PadState[1].joy == NULL)
		|| (PadMapping[2].enabled && PadState[2].joy == NULL)
		|| (PadMapping[3].enabled && PadState[3].joy == NULL))
	{
		g_PADInitialize->padNumber = -1;
		Console::Print("%s\n", SDL_GetError());		
	}
}

 
// Search attached devices. Populate joyinfo for all attached physical devices.
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
int Search_Devices()
{
	// Load config
	#ifdef _DEBUG
		DEBUG_INIT();
	#endif

	/* SDL 1.3 use DirectInput instead of the old Microsoft Multimeda API, and with this we need 
	   the SDL_INIT_VIDEO flag to */
	if (!SDL_WasInit(0))
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
		{
			PanicAlert("Could not initialize SDL: %s", SDL_GetError());
			return 0;
		}

	#ifdef _DEBUG
		fprintf(pFile, "Scanning for devices\n");
		fprintf(pFile, "ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ\n");
	#endif

	// Get device status
	int numjoy = SDL_NumJoysticks();
	for (int i = 0; i < numjoy; i++ )
	{
		CONTROLLER_INFO Tmp;

		Tmp.joy			= SDL_JoystickOpen(i);
		Tmp.ID			= i;
		Tmp.NumAxes		= SDL_JoystickNumAxes(Tmp.joy);
		Tmp.NumButtons	= SDL_JoystickNumButtons(Tmp.joy);
		Tmp.NumBalls	= SDL_JoystickNumBalls(Tmp.joy);
		Tmp.NumHats		= SDL_JoystickNumHats(Tmp.joy);
		Tmp.Name		= SDL_JoystickName(i);

		// Check if the device is okay
		if (   Tmp.NumAxes == 0
			&&  Tmp.NumBalls == 0
			&&  Tmp.NumButtons == 0
			&&  Tmp.NumHats == 0
			)
		{
			Tmp.Good = false;
		}
		else
		{
			NumGoodPads++;
			Tmp.Good = true;
		}

		joyinfo.push_back(Tmp);

		#ifdef _DEBUG
			fprintf(pFile, "ID: %d\n", i);
			fprintf(pFile, "Name: %s\n", joyinfo[i].Name);
			fprintf(pFile, "Buttons: %d\n", joyinfo[i].NumButtons);
			fprintf(pFile, "Axes: %d\n", joyinfo[i].NumAxes);
			fprintf(pFile, "Hats: %d\n", joyinfo[i].NumHats);
			fprintf(pFile, "Balls: %d\n\n", joyinfo[i].NumBalls);
		#endif

		// We have now read the values we need so we close the device
		if (SDL_JoystickOpened(i)) SDL_JoystickClose(joyinfo[i].joy);
	}

	// Warn the user if no gamepads are detected
	if (NumGoodPads == 0 && emulator_running)
	{
		PanicAlert("No Joystick detected");
		return joyinfo.size();
	}

	// Load PadMapping[] etc
	g_Config.Load();

	// Update the PadState[].joy handle
	for (int i = 0; i < 4; i++)
	{
		if (PadMapping[i].enabled && joyinfo.size() > PadMapping[i].ID)
			if(joyinfo.at(PadMapping[i].ID).Good)
				PadState[i].joy = SDL_JoystickOpen(PadMapping[i].ID);
	}

	return joyinfo.size();
}

// Shutdown PAD (stop emulation)
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
/* Information: This function can not be run twice without an Initialize in between. If
   it's run twice the SDL_...() functions below will cause a crash.
   Called from: The Dolphin Core, ConfigBox::OnClose() */
void Shutdown()
{
	Console::Print("Shutdown: %i\n", SDL_WasInit(0));

	/* Close all devices carefully. We must check that we are not accessing any undefined
	   vector elements or any bad devices */
	for (int i = 0; i < 4; i++)
	{
		if (PadMapping[i].enabled && joyinfo.size() > PadMapping[i].ID)
			if (joyinfo.at(PadMapping[i].ID).Good)
				if(SDL_JoystickOpened(PadMapping[i].ID)) SDL_JoystickClose(PadState[i].joy);
	}

	SDL_Quit();

	#ifdef _DEBUG
		DEBUG_QUIT();
	#endif

	// Clear the physical device info
	//delete [] joyinfo;
	//joyinfo = NULL;
	joyinfo.clear();

	emulator_running = false;

	#ifdef _WIN32
		#ifdef USE_RUMBLE_DINPUT_HACK
			FreeDirectInput();
		#endif
	#elif defined(__linux__)
		close(fd);
	#endif
}


// Set buttons status from keyboard input. Currently this is done from wxWidgets in the main application.
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void PAD_Input(u16 _Key, u8 _UpDown)
{
	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (!IsFocus()) return;

	// Check if the keys are interesting, and then update it
	for(int i = 0; i < 4; i++)
	{
		for(int j = CTL_L_SHOULDER; j <= CTL_START; j++)
		{
			if (PadMapping[i].buttons[j] == _Key)
				{ PadState[i].buttons[j] = _UpDown; break; }
		}

		for(int j = CTL_D_PAD_UP; j <= CTL_D_PAD_RIGHT; j++)
		{
			if (PadMapping[i].dpad2[j] == _Key)
				{ PadState[i].dpad2[j] = _UpDown; break; }
		}
	}

	// Debugging
	//Console::Print("%i", _Key);
}


// Save state
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void DoState(unsigned char **ptr, int mode) {}

 
// Set PAD attached pads
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;

	g_Config.Load();

	if (PadMapping[0].enabled) connected |= 1;		
	if (PadMapping[1].enabled) connected |= 2;
	if (PadMapping[2].enabled) connected |= 4;
	if (PadMapping[3].enabled) connected |= 8;

	//Console::Print("PAD_GetAttachedPads: %i %i %i\n", PadMapping[0].enabled, PadMapping[1].enabled, PadMapping[2].enabled, PadMapping[3].enabled);

	return connected;
}


// Set PAD status
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
// Called from: SI_DeviceGCController.cpp
// Function: Gives the current pad status to the Core
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	//Console::Print("%i %i %i\n", _numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy);

	// Check if the pad is enabled
	if (!PadMapping[_numPAD].enabled || !PadState[_numPAD].joy) return;

	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Update the pad status
	GetJoyState(_numPAD);

	// Get type
	int TriggerType = PadMapping[_numPAD].triggertype;
 
	///////////////////////////////////////////////////
	// The analog controls
	// -----------

	// Read axis values
	int i_main_stick_x = PadState[_numPAD].axis[CTL_MAIN_X];
	int i_main_stick_y = -PadState[_numPAD].axis[CTL_MAIN_Y];
    int i_sub_stick_x = PadState[_numPAD].axis[CTL_SUB_X];
	int i_sub_stick_y = -PadState[_numPAD].axis[CTL_SUB_Y];
	int TriggerLeft = PadState[_numPAD].axis[CTL_L_SHOULDER];
	int TriggerRight = PadState[_numPAD].axis[CTL_R_SHOULDER];

	// Check if we should make adjustments
	if(PadMapping[_numPAD].bSquareToCircle)
	{
		std::vector<int> main_xy = Pad_Square_to_Circle(i_main_stick_x, i_main_stick_y, _numPAD);
		i_main_stick_x = main_xy.at(0);
		i_main_stick_y = main_xy.at(1);
	}

	// Convert axis values
	u8 main_stick_x = Pad_Convert(i_main_stick_x);
	u8 main_stick_y = Pad_Convert(i_main_stick_y);
    u8 sub_stick_x = Pad_Convert(i_sub_stick_x);
	u8 sub_stick_y = Pad_Convert(i_sub_stick_y);

	// Convert the triggers values, if we are using analog triggers at all
	if(PadMapping[_numPAD].triggertype == CTL_TRIGGER_SDL)
	{
		if(PadMapping[_numPAD].buttons[CTL_L_SHOULDER] >= 1000) TriggerLeft = Pad_Convert(TriggerLeft);
		if(PadMapping[_numPAD].buttons[CTL_R_SHOULDER] >= 1000) TriggerRight = Pad_Convert(TriggerRight);
	}

	// Set Deadzones (perhaps out of function?)
	int deadzone = (int)(((float)(128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone + 1));
	int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone + 1));

	// Send values to Dolpin if they are outside the deadzone
	if ((main_stick_x < deadzone2)	|| (main_stick_x > deadzone))	_pPADStatus->stickX = main_stick_x;
	if ((main_stick_y < deadzone2)	|| (main_stick_y > deadzone))	_pPADStatus->stickY = main_stick_y;
	if ((sub_stick_x < deadzone2)	|| (sub_stick_x > deadzone))	_pPADStatus->substickX = sub_stick_x;
	if ((sub_stick_y < deadzone2)	|| (sub_stick_y > deadzone))	_pPADStatus->substickY = sub_stick_y;

 
	///////////////////////////////////////////////////
	// The L and R triggers
	// -----------	
	int TriggerValue = 255;
	if (PadState[_numPAD].halfpress) TriggerValue = 100;

	_pPADStatus->button |= PAD_USE_ORIGIN; // Neutral value, no button pressed	

	// Check if the digital L button is pressed
	if (PadState[_numPAD].buttons[CTL_L_SHOULDER])
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = TriggerValue;
	}
	else if(TriggerLeft > 0)
		_pPADStatus->triggerLeft = TriggerLeft;

	// Check if the digital R button is pressed
	if (PadState[_numPAD].buttons[CTL_R_SHOULDER])
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = TriggerValue;
	}
	else if(TriggerRight > 0)
		_pPADStatus->triggerRight = TriggerRight;

	// Update the buttons in analog mode to
	if(TriggerLeft == 0xff) _pPADStatus->button |= PAD_TRIGGER_L;
	if(TriggerRight == 0xff) _pPADStatus->button |= PAD_TRIGGER_R;


	///////////////////////////////////////////////////
	// The digital buttons
	// -----------	
	if (PadState[_numPAD].buttons[CTL_A_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[CTL_B_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[CTL_X_BUTTON])	_pPADStatus->button|=PAD_BUTTON_X;
	if (PadState[_numPAD].buttons[CTL_Y_BUTTON])	_pPADStatus->button|=PAD_BUTTON_Y;
	if (PadState[_numPAD].buttons[CTL_Z_TRIGGER])	_pPADStatus->button|=PAD_TRIGGER_Z;
	if (PadState[_numPAD].buttons[CTL_START])		_pPADStatus->button|=PAD_BUTTON_START;


	///////////////////////////////////////////////////
	// The D-pad
	// -----------		
	if (PadMapping[_numPAD].controllertype == CTL_DPAD_HAT)
	{
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_UP		|| PadState[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_LEFT	|| PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN	|| PadState[_numPAD].dpad == SDL_HAT_DOWN	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad == SDL_HAT_RIGHTUP	|| PadState[_numPAD].dpad == SDL_HAT_RIGHT	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_RIGHT;
	}
	else
	{
		if (PadState[_numPAD].dpad2[CTL_D_PAD_UP])
			_pPADStatus->button |= PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad2[CTL_D_PAD_DOWN])
			_pPADStatus->button |= PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad2[CTL_D_PAD_LEFT])
			_pPADStatus->button |= PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad2[CTL_D_PAD_RIGHT])
			_pPADStatus->button |= PAD_BUTTON_RIGHT;
	}

	// Update error code
	_pPADStatus->err = PAD_ERR_NONE;

	// Use rumble
	Pad_Use_Rumble(_numPAD, _pPADStatus);

	/* Debugging 
	Console::ClearScreen();
	Console::Print(
		"Pad %i: %i %i\n"
		"State: L:%i R:%i HalfPress:%i\n"
		"Trigger type: %s  StatusLeft:%04x StatusRight:%04x  TriggerLeft:%04x TriggerRight:%04x  TriggerValue:%i\n"
		"Buttons: %i  X:%i\n"
		"D-Pad type: %s  L:%i  R:%i  U:%i  D:%i",

		_numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy,

		 PadState[_numPAD].buttons[CTL_L_SHOULDER], PadState[_numPAD].buttons[CTL_R_SHOULDER], PadState[_numPAD].halfpress,

		(PadMapping[_numPAD].triggertype ? "CTL_TRIGGER_XINPUT" : "CTL_TRIGGER_SDL"),
			_pPADStatus->triggerLeft, _pPADStatus->triggerRight,  TriggerLeft, TriggerRight,  TriggerValue,

		_pPADStatus->button, PadState[_numPAD].buttons[CTL_X_BUTTON],

		(PadMapping[_numPAD].controllertype ? "CTL_DPAD_CUSTOM" : "CTL_DPAD_HAT"),
			0, 0, 0, 0
		);*/
}


///////////////////////////////////////////////// Spec functions



//******************************************************************************
// Supporting functions
//******************************************************************************

//////////////////////////////////////////////////////////////////////////////////////////
// Check if Dolphin is in focus
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
bool IsFocus()
{
#ifdef _WIN32
	HWND RenderingWindow = NULL; if (g_PADInitialize) RenderingWindow = g_PADInitialize->hWnd;
	HWND Parent = GetParent(RenderingWindow);
	HWND TopLevel = GetParent(Parent);
	HWND Config = NULL; if (m_frame) Config = (HWND)m_frame->GetHWND();
	// Support both rendering to main window and not
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == RenderingWindow || GetForegroundWindow() == Config)
		return true;
	else
		return false;
#else
	return true;
#endif
}


//////////////////////////////////////////////////////////////////////////////////////////
// Convert stick values
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ

/* Convert stick values.

   The value returned by SDL_JoystickGetAxis is a signed integer s16
   (-32768 to 32767). The value used for the gamecube controller is an unsigned
   char u8 (0 to 255) with neutral at 0x80 (128), so that it's equivalent to a signed
   -128 to 127.
*/
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
int Pad_Convert(int _val)
{
	/* If the limits on PadState[].axis[] actually is a u16 then we don't need this
	   but if it's not actually limited to that we need to apply these limits */
	if(_val > 32767) _val = 32767; // upper limit
	if(_val < -32768) _val = -32768; // lower limit
		
	// Convert the range (-0x8000 to 0x7fff) to (0 to 0xffff)
	_val = 0x8000 +_val;

	// Convert the range (-32768 to 32767) to (-128 to 127)
	_val = _val >> 8;

	//Console::Print("0x%04x  %06i\n\n", _val, _val);

	return _val;
}

 
/* Convert the stick raidus from a circular to a square. I don't know what input values
   the actual GC controller produce for the GC, it may be a square, a circle or something
   in between. But one thing that is certain is that PC pads differ in their output (as
   shown in the list below), so it may be beneficiary to convert whatever radius they
   produce to the radius the GC games expect. This is the first implementation of this
   that convert a square radius to a circual radius. Use the advanced settings to enable
   and calibrate it.

   Observed diagonals:
   Perfect circle: 71% = sin(45)
   Logitech Dual Action: 100%
   Dual Shock 2 (Original) with Super Dual Box Pro: 90%
   XBox 360 Wireless: 85%
   GameCube Controller (Third Party) with EMS TrioLinker Plus II: 60%
*/
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
float SquareDistance(float deg)
{
	// See if we have to adjust the angle
	deg = abs(deg);
	if( (deg > 45 && deg < 135) ) deg = deg - 90;

	float rad = deg * M_PI / 180;

	float val = abs(cos(rad));
	float dist = 1 / val; // Calculate distance from center

	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("Deg:%f  Val:%f  Dist:%f", deg, val, dist));

	return dist;
}
std::vector<int> Pad_Square_to_Circle(int _x, int _y, int _pad)
{
	/* Do we need this? */
	if(_x > 32767) _x = 32767; if(_y > 32767) _y = 32767; // upper limit
	if(_x < -32768) _x = -32768; if(_y > 32767) _y = 32767; // lower limit

	// ====================================
	// Convert to circle
	// -----------
	int Tmp = atoi (PadMapping[_pad].SDiagonal.substr(0, PadMapping[_pad].SDiagonal.length() - 1).c_str());
	float Diagonal = Tmp / 100.0;

	// First make a perfect square in case we don't have one already
	float OrigDist = sqrt(  pow((float)_y, 2) + pow((float)_x, 2)  ); // Get current distance
	float rad = atan2((float)_y, (float)_x); // Get current angle
	float deg = rad * 180 / M_PI;

	// A diagonal of 85% means a distance of 1.20
	float corner_circle_dist = ( Diagonal / sin(45 * M_PI / 180) );
	float SquareDist = SquareDistance(deg);
	float adj_ratio1; // The original-to-square distance adjustment
	float adj_ratio2 = SquareDist; // The circle-to-square distance adjustment
	// float final_ratio; // The final adjustment to the current distance //TODO: This is not used
	float result_dist; // The resulting distance

	// Calculate the corner-to-square adjustment ratio
	if(corner_circle_dist < SquareDist) adj_ratio1 = SquareDist / corner_circle_dist;
		else adj_ratio1 = 1;

	// Calculate the resulting distance
	result_dist = OrigDist * adj_ratio1 / adj_ratio2;

	float x = result_dist * cos(rad); // calculate x
	float y = result_dist * sin(rad); // calculate y

	int int_x = (int)floor(x);
	int int_y = (int)floor(y);

	// Debugging
	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("%f  %f  %i", corner_circle_dist, Diagonal, Tmp));

	std::vector<int> vec;
	vec.push_back(int_x);
	vec.push_back(int_y);
	return vec;
}
///////////////////////////////////////////////////////////////////// Convert stick values

 

//////////////////////////////////////////////////////////////////////////////////////////
// Supporting functions
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ

// Read current joystick status
/* ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
	The value PadMapping[].buttons[] is the number of the assigned joypad button,
	PadState[].buttons[] is the status of the button, it becomes 0 (no pressed) or 1 (pressed) */


// Read buttons status. Called from GetJoyState().
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
void ReadButton(int controller, int button)
{
	int ctl_button = PadMapping[controller].buttons[button];
	if (ctl_button < joyinfo[PadMapping[controller].ID].NumButtons)
	{
		PadState[controller].buttons[button] = SDL_JoystickGetButton(PadState[controller].joy, ctl_button);
	}
}

// Request joystick state.
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
/* Called from: PAD_GetStatus()
   Input: The virtual device 0, 1, 2 or 3
   Function: Updates the PadState struct with the current pad status. The input value "controller" is
   for a virtual controller 0 to 3. */
void GetJoyState(int controller)
{
	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (!g_Config.bCheckFocus && !IsFocus()) return;

	// Update the gamepad status
	SDL_JoystickUpdate();

	// Save the number of buttons
	int Buttons = joyinfo[PadMapping[controller].ID].NumButtons;

	// Update axis states. It doesn't hurt much if we happen to ask for nonexisting axises here.
	PadState[controller].axis[CTL_MAIN_X] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].axis[CTL_MAIN_X]);
	PadState[controller].axis[CTL_MAIN_Y] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].axis[CTL_MAIN_Y]);
	PadState[controller].axis[CTL_SUB_X] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].axis[CTL_SUB_X]);
	PadState[controller].axis[CTL_SUB_Y] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].axis[CTL_SUB_Y]);

	// Update the analog trigger axis values
#ifdef _WIN32
	if (PadMapping[controller].triggertype == CTL_TRIGGER_SDL)
	{
#endif
		// If we are using SDL analog triggers the buttons have to be mapped as 1000 or up, otherwise they are not used
		if(PadMapping[controller].buttons[CTL_L_SHOULDER] >= 1000) PadState[controller].axis[CTL_L_SHOULDER] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].buttons[CTL_L_SHOULDER] - 1000); else PadState[controller].axis[CTL_L_SHOULDER] = 0;
		if(PadMapping[controller].buttons[CTL_R_SHOULDER] >= 1000) PadState[controller].axis[CTL_R_SHOULDER] = SDL_JoystickGetAxis(PadState[controller].joy, PadMapping[controller].buttons[CTL_R_SHOULDER] - 1000); else PadState[controller].axis[CTL_R_SHOULDER] = 0;
#ifdef _WIN32
	}
	else
	{
		PadState[controller].axis[CTL_L_SHOULDER] = XInput::GetXI(0, PadMapping[controller].buttons[CTL_L_SHOULDER] - 1000);
		PadState[controller].axis[CTL_R_SHOULDER] = XInput::GetXI(0, PadMapping[controller].buttons[CTL_R_SHOULDER] - 1000);
	}
#endif

	// Update button states to on or off
	ReadButton(controller, CTL_L_SHOULDER);
	ReadButton(controller, CTL_R_SHOULDER);
	ReadButton(controller, CTL_A_BUTTON);
	ReadButton(controller, CTL_B_BUTTON);
	ReadButton(controller, CTL_X_BUTTON);
	ReadButton(controller, CTL_Y_BUTTON);
	ReadButton(controller, CTL_Z_TRIGGER);
	ReadButton(controller, CTL_START);

	//
	if (PadMapping[controller].halfpress < joyinfo[controller].NumButtons)
		PadState[controller].halfpress = SDL_JoystickGetButton(PadState[controller].joy, PadMapping[controller].halfpress);

	// Check if we have an analog or digital joypad
	if (PadMapping[controller].controllertype == CTL_DPAD_HAT)
	{
		PadState[controller].dpad = SDL_JoystickGetHat(PadState[controller].joy, PadMapping[controller].dpad);
	}
	else
	{
		/* Only do this if the assigned button is in range (to allow for the current way of saving keyboard
		   keys in the same array) */
		if(PadMapping[controller].dpad2[CTL_D_PAD_UP] <= Buttons)
			PadState[controller].dpad2[CTL_D_PAD_UP] = SDL_JoystickGetButton(PadState[controller].joy, PadMapping[controller].dpad2[CTL_D_PAD_UP]);
		if(PadMapping[controller].dpad2[CTL_D_PAD_DOWN] <= Buttons)
			PadState[controller].dpad2[CTL_D_PAD_DOWN] = SDL_JoystickGetButton(PadState[controller].joy, PadMapping[controller].dpad2[CTL_D_PAD_DOWN]);
		if(PadMapping[controller].dpad2[CTL_D_PAD_LEFT] <= Buttons)
			PadState[controller].dpad2[CTL_D_PAD_LEFT] = SDL_JoystickGetButton(PadState[controller].joy, PadMapping[controller].dpad2[CTL_D_PAD_LEFT]);
		if(PadMapping[controller].dpad2[CTL_D_PAD_RIGHT] <= Buttons)
			PadState[controller].dpad2[CTL_D_PAD_RIGHT] = SDL_JoystickGetButton(PadState[controller].joy, PadMapping[controller].dpad2[CTL_D_PAD_RIGHT]);
	}

	/* Debugging 
	Console::ClearScreen();
	Console::Print(
		"Controller and handle: %i %i\n"

		"Triggers:%i  %i %i  %i %i  |  HalfPress: %i Mapping: %i\n",

		controller, (int)PadState[controller].joy,

		PadMapping[controller].triggertype,
		PadMapping[controller].buttons[CTL_L_SHOULDER], PadMapping[controller].buttons[CTL_R_SHOULDER],
		PadState[controller].axis[CTL_L_SHOULDER], PadState[controller].axis[CTL_R_SHOULDER],

		PadState[controller].halfpress, PadMapping[controller].halfpress
		);	*/
}
//////////////////////////////////////////////////////////////////////////////////////////
