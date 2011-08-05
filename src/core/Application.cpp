/*
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//////////////////////////////////////////////////////////////////////////////////////
//   @@        @@@        @@@                @@                           @@@@@     //
//   @@@       @@@@@@     @@@     @@        @@@@                         @@@  @@@   //
//   @@@       @@@@@@@    @@@    @@@@       @@@@      @@                @@@@        //
//   @@@       @@  @@@@   @@@  @@@@@       @@@@@@     @@@               @@@         //
//  @@@@@      @@  @@@@   @@@ @@@@@        @@@@@@@    @@@            @  @@@         //
//  @@@@@      @@  @@@@  @@@@@@@@         @@@@ @@@    @@@@@         @@ @@@@@@@      //
//  @@ @@@     @@  @@@@  @@@@@@@          @@@  @@@    @@@@@@        @@ @@@@         //
// @@@ @@@    @@@ @@@@   @@@@@            @@@@@@@@@   @@@@@@@      @@@ @@@@         //
// @@@ @@@@   @@@@@@@    @@@@@@           @@@  @@@@   @@@ @@@      @@@ @@@@         //
// @@@@@@@@   @@@@@      @@@@@@@@@@      @@@    @@@   @@@  @@@    @@@  @@@@@        //
// @@@  @@@@  @@@@       @@@  @@@@@@@    @@@    @@@   @@@@  @@@  @@@@  @@@@@        //
//@@@   @@@@  @@@@@      @@@      @@@@@@ @@     @@@   @@@@   @@@@@@@    @@@@@ @@@@@ //
//@@@   @@@@@ @@@@@     @@@@        @@@  @@      @@   @@@@   @@@@@@@    @@@@@@@@@   //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@      @@@@@      //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@       @@        //
//@@@    @@@  @@@ @@@@@                          @@            @@@                  //
//            @@@ @@@                           @@             @@        STUDIOS    //
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// EERIEApp
//////////////////////////////////////////////////////////////////////////////////////
//
// Description:
//
// Updates: (date) (person) (update)
//
// Code:	Cyril Meynier
//			S�bastien Scieux	(Zbuffer)
//			Didier Pedreno		(ScreenSaver Problem Fix)
//
// Copyright (c) 1999 ARKANE Studios SA. All rights reserved
//////////////////////////////////////////////////////////////////////////////////////

#include "core/Application.h"

#include <cstdio>

#include "core/Config.h"
#include "core/Resource.h"
#include "core/GameTime.h"

#include "game/Player.h"

#include "gui/Menu.h"
#include "gui/Interface.h"
#include "gui/MenuWidgets.h"

#include "graphics/GraphicsUtility.h"
#include "graphics/Frame.h"
#include "graphics/GraphicsEnum.h"
#include "graphics/data/Mesh.h"

#include "input/Input.h"

#include "io/FilePath.h"
#include "io/PakManager.h"
#include "io/Logger.h"

#include "platform/Random.h"

using std::max;

//-----------------------------------------------------------------------------
long EERIEMouseButton = 0;
long LastEERIEMouseButton = 0;
long EERIEMouseGrab = 0;

Application* mainApp = 0;
EERIE_CAMERA		* Kam;
LPDIRECT3DDEVICE7	GDevice;
HWND				MSGhwnd = NULL;
float				FPS;
LightMode ModeLight = 0;
ViewModeFlags ViewMode = 0;

static int iCurrZBias;


//*************************************************************************************
// Application()
// Constructor
//*************************************************************************************
Application::Application()
{
	m_pFramework   = NULL;
	m_pD3D         = NULL;

	lpDDGammaControl = NULL;

	m_bReady          = false;

	m_bAppUseZBuffer  = false;
}

bool Application::Initialize()
{
	bool init;

	init = InitConfig();
	if(!init) {
		return false;
	}

	init = InitWindow();
	if(!init) {
		return false;
	}

	init = InitGraphics();
	if(!init) {
		return false;
	}

	init = InitInput();
	if(!init) {
		return false;
	}

	init = InitSound();
	if(!init) {
		return false;
	}

	Random::seed();

	return true;
}

bool Application::InitConfig()
{
	// Initialize config first, before anything else.
	const char RESOURCE_CONFIG[] = "cfg.ini";
	const char RESOURCE_CONFIG_DEFAULT[] = "cfg_default.ini";
	if(!config.init(RESOURCE_CONFIG, RESOURCE_CONFIG_DEFAULT)) {
		LogWarning << "Could not read config files " << RESOURCE_CONFIG << " and " << RESOURCE_CONFIG_DEFAULT;
		return false;
	}

	return true;
}

void Application::EvictManagedTextures()
{
	if (this->m_pD3D)
	{
		this->m_pD3D->EvictManagedTextures();
	}
}


//*************************************************************************************
// Pause()
// Called in to toggle the pause state of the app. This function
// brings the GDI surface to the front of the display, so drawing
// output like message boxes and menus may be displayed.
//*************************************************************************************
void Application::Pause(bool bPause)
{
	static DWORD dwAppPausedCount = 0L;

	dwAppPausedCount += (bPause ? +1 : -1);
	m_bReady          = (dwAppPausedCount ? false : true);

	// Handle the first pause request (of many, nestable pause requests)
	if (bPause && (1 == dwAppPausedCount))
	{
		// Get a surface for the GDI
		if (m_pFramework)
			m_pFramework->FlipToGDISurface(true);
	}
}

//*************************************************************************************
//*************************************************************************************
VOID CalcFPS(bool reset)
{
	static float fFPS      = 0.0f;
	static float fLastTime = 0.0f;
	static DWORD dwFrames  = 0L;

	if (reset)
	{
		dwFrames = 0;
		fLastTime = 0.f;
		FPS = fFPS = 7.f * FPS;
	}
	else
	{
		float tmp;

		// Keep track of the time lapse and frame count
		float fTime = ARX_TIME_Get(false) * 0.001f;   // Get current time in seconds
		++dwFrames;

		tmp = fTime - fLastTime;

		// Update the frame rate once per second
		if (tmp > 1.f)
		{
			FPS = fFPS      = dwFrames / tmp ;
			fLastTime = fTime;
			dwFrames  = 0L;
		}
	}
}


//******************************************************************************
// MESSAGE BOXES
//******************************************************************************
//*************************************************************************************
//*************************************************************************************
//TODO(lubosz): is this needed in the game? replace
bool OKBox(const std::string& text, const std::string& title)

{
	int i;
	mainApp->Pause(true);
	i = MessageBox((HWND)mainApp->GetWindow()->GetHandle(), text.c_str(), title.c_str(), MB_ICONQUESTION | MB_OKCANCEL);
	mainApp->Pause(false);

	if (i == IDCANCEL) return false;

	return true;
}

void SetZBias(int _iZBias)
{
	if (_iZBias < 0)
	{
		_iZBias = 0;
		_iZBias = max(iCurrZBias, -_iZBias);
	}

	if (_iZBias == iCurrZBias) 
		return;

	iCurrZBias = _iZBias;

	GRenderer->SetDepthBias(iCurrZBias);
}
