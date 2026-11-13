//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The panel that sits under all the other GameUI panels and draws the background
//
// $NoKeywords: $
//=============================================================================

#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include "BasePanel.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "Taskbar.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBasePanel::CBasePanel() : Panel(NULL, "BasePanel")
{
	m_eBackgroundState = BACKGROUND_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: notifications
//-----------------------------------------------------------------------------
void CBasePanel::OnChildAdded(VPANEL child)
{
	if (g_pTaskbar)
	{
		g_pTaskbar->AddTask(child);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes the panel invisible to clicks when it is hidden
//-----------------------------------------------------------------------------
VPANEL CBasePanel::IsWithinTraverse(int x, int y, bool traversePopups)
{
	// don't return anything if we're invisible
	if (!IsVisible())
		return NULL;

	return Panel::IsWithinTraverse(x, y, false);
}

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBasePanel::PaintBackground()
{
	const char *levelName = engine->pfnGetLevelName();
	if (levelName && levelName[0] && m_eBackgroundState != BACKGROUND_CAREERLOAD)
	{
		// render filled background in game
		int swide, stall;
		surface()->GetScreenSize(swide, stall);
		surface()->DrawSetColor(0, 0, 0, 128);
		surface()->DrawFilledRect(0, 0, swide, stall);
		return;
	}

	switch (m_eBackgroundState)
	{
	case BACKGROUND_BLACK:
	case BACKGROUND_CAREERLOAD:
		{
			// if the loading dialog is visible, draw the background black
			int swide, stall;
			surface()->GetScreenSize(swide, stall);
			surface()->DrawSetColor(0, 0, 0, 255);
			surface()->DrawFilledRect(0, 0, swide, stall);
		}
		break;

	case BACKGROUND_DESKTOPIMAGE:
		DrawBackgroundImage();
		break;

	case BACKGROUND_LOADING:
		DrawBackgroundImage();
		break;

	case BACKGROUND_LOADINGTRANSITION:
		{
		}
		break;

	case BACKGROUND_NONE:
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads background texture
//-----------------------------------------------------------------------------
void CBasePanel::ApplySchemeSettings(IScheme *pScheme)
{
	Panel::ApplySchemeSettings(pScheme);

	// load the background layout
	FileHandle_t file = filesystem()->Open("resource/BackgroundLayout.txt", "rt");
	if (!file)
		return;

	// read the whole file into memory
	int fileSize = filesystem()->Size(file);
	char *buffer = (char *)_alloca(fileSize + 1);
	filesystem()->Read(buffer, fileSize, file);
	filesystem()->Close(file);
	buffer[fileSize] = 0;

	// parse
	char token[512];
	while (buffer && *buffer && (buffer = filesystem()->ParseFile(buffer, token, NULL)) != NULL && token[0])
	{
		if (!stricmp(token, "resolution"))
		{
			// get the resolution
			buffer = filesystem()->ParseFile(buffer, token, NULL);
			m_iBaseResX = atoi(token);
			buffer = filesystem()->ParseFile(buffer, token, NULL);
			m_iBaseResY = atoi(token);
		}
		else
		{
			// get the image
			bimage_t &bimage = m_ImageID[m_ImageID.AddToTail()];
			bimage.imageID = surface()->CreateNewTextureID(false);

			// remove the .tga extension
			char *ext = strstr(token, ".tga");
			if (ext)
			{
				*ext = 0;
			}
			surface()->DrawSetTextureFile(bimage.imageID, token, false, false);
			surface()->DrawGetTextureSize(bimage.imageID, bimage.width, bimage.height);

			// parse the scale
			buffer = filesystem()->ParseFile(buffer, token, NULL);
			bimage.scaled = !stricmp(token, "scaled");

			// parse the position
			buffer = filesystem()->ParseFile(buffer, token, NULL);
			bimage.x = atoi(token);
			buffer = filesystem()->ParseFile(buffer, token, NULL);
			bimage.y = atoi(token);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets how the game background should render
//-----------------------------------------------------------------------------
void CBasePanel::SetBackgroundRenderState(EBackgroundState state)
{
	m_eBackgroundState = state;
}

//-----------------------------------------------------------------------------
// Purpose: draws the background pieces scaled to the screen
//-----------------------------------------------------------------------------
void CBasePanel::DrawBackgroundImage()
{
	int wide, tall;
	GetSize(wide, tall);

	// work out scaling factors
	int swide, stall;
	surface()->GetScreenSize(swide, stall);
	float xScale, yScale;
	xScale = swide / (float)m_iBaseResX;
	yScale = stall / (float)m_iBaseResY;

	// iterate and draw all the background pieces
	for (int x = 0; x < m_ImageID.Count(); x++)
	{
		bimage_t &bimage = m_ImageID[x];

		int dx = bimage.x ? (int)ceil(bimage.x * xScale) : 0;
		int dy = bimage.y ? (int)ceil(bimage.y * yScale) : 0;
		int dw, dt;
		if (bimage.scaled)
		{
			dw = (int)ceil((bimage.x + bimage.width) * xScale);
			dt = (int)ceil((bimage.y + bimage.height) * yScale);
		}
		else
		{
			dw = dx + bimage.width;
			dt = dy + bimage.height;
		}

		surface()->DrawSetColor(255, 255, 255, 255);
		surface()->DrawSetTexture(bimage.imageID);
		surface()->DrawTexturedRect(dx, dy, dw, dt);
	}
}
