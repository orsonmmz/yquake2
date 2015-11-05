/*
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is the Quake II input system backend, implemented with SDL.
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"
#include "../../client/header/keyboard.h"
#include "../generic/header/input.h"
#include "../../client/header/client.h"

#define MOUSE_MAX 3000
#define MOUSE_MIN 40

/* Globals */
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean have_grab;
static qboolean mlooking;

/* CVars */
cvar_t *vid_fullscreen;
static cvar_t *in_grab;
static cvar_t *in_mouse;
static cvar_t *exponential_speedup;
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
static cvar_t *m_filter;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_yaw;
cvar_t *sensitivity;
static cvar_t *windowed_mouse;

/* ------------------------------------------------------------------ */

/*
 * Updates the input queue state. Called every
 * frame by the client and does nearly all the
 * input magic.
 */
void
IN_Update(void)
{
	qboolean want_grab;
 	unsigned int key;

        // TODO orson

#if 0
	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{

		switch (event.type)
		{
#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_MOUSEWHEEL:
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
				break;
#endif
			case SDL_MOUSEBUTTONDOWN:
#if !SDL_VERSION_ATLEAST(2, 0, 0)
				if (event.button.button == 4)
				{
					Key_Event(K_MWHEELUP, true, true);
					Key_Event(K_MWHEELUP, false, true);
					break;
				}
				else if (event.button.button == 5)
				{
					Key_Event(K_MWHEELDOWN, true, true);
					Key_Event(K_MWHEELDOWN, false, true);
					break;
				}
#endif
			case SDL_MOUSEBUTTONUP:
				switch( event.button.button )
				{
					case SDL_BUTTON_LEFT:
						key = K_MOUSE1;
						break;
					case SDL_BUTTON_MIDDLE:
						key = K_MOUSE3;
						break;
					case SDL_BUTTON_RIGHT:
						key = K_MOUSE2;
						break;
					case SDL_BUTTON_X1:
						key = K_MOUSE4;
						break;
					case SDL_BUTTON_X2:
						key = K_MOUSE5;
						break;
					default:
						return;
				}

				Key_Event(key, (event.type == SDL_MOUSEBUTTONDOWN), true);
				break;

			case SDL_MOUSEMOTION:
#if SDL_VERSION_ATLEAST(2, 0, 0)
				/* This is a hack to work around an unsuccessful
				   SDL_SetRelativeMouseMode(). This can happen if
				   some broken security software is blocking raw
				   input (to prevent keyloggers accessing the input
				   queue), or if - on Linux / Unix - XInput2 is not
				   available.

				   Since SDL_WarpMouseInWindow() injects a movement
				   event into the queue, we ignore events that move
				   the mouse exactly to the warp position.

				   The underlying issue _should_ be solved in SDL
				   2.0.3 and above. */
				if (have_grab && !in_relativemode)
				{
					int center_x = vid.width / 2;
					int center_y = vid.height / 2;
					if (event.motion.x == center_x && event.motion.y == center_y)
					{
						break;
					}
					SDL_WarpMouseInWindow(NULL, center_x, center_y);
				}
#endif
                if (cls.key_dest == key_game && (int)cl_paused->value == 0) {
                    mouse_x += event.motion.xrel;
                    mouse_y += event.motion.yrel;
                }
				break;

#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_TEXTINPUT:
				if ((event.text.text[0] >= SDLK_SPACE) &&
					 (event.text.text[0] < SDLK_DELETE))
				{
					Char_Event(event.text.text[0]);
				}

				break;
#endif

			case SDL_KEYDOWN:
#if !SDL_VERSION_ATLEAST(2, 0, 0)
				if ((event.key.keysym.unicode >= SDLK_SPACE) &&
					 (event.key.keysym.unicode < SDLK_DELETE))
				{
					Char_Event(event.key.keysym.unicode);
				}
#endif

				if ((event.key.keysym.sym >= SDLK_SPACE) &&
					 (event.key.keysym.sym < SDLK_DELETE))
				{
					Key_Event(event.key.keysym.sym, true, false);
				}
				else
				{
					Key_Event(IN_TranslateSDLtoQ2Key(event.key.keysym.sym), true, true);
				}

				break;

			case SDL_KEYUP:

				if ((event.key.keysym.sym >= SDLK_SPACE) &&
					 (event.key.keysym.sym < SDLK_DELETE))
				{
					Key_Event(event.key.keysym.sym, false, false);
				}
				else
				{
					Key_Event(IN_TranslateSDLtoQ2Key(event.key.keysym.sym), false, true);
				}

				break;
		}
	}
#endif

	/* Grab and ungrab the mouse if the* console or the menu is opened */
	want_grab = (vid_fullscreen->value || in_grab->value == 1 ||
			(in_grab->value == 2 && windowed_mouse->value));

	if (have_grab != want_grab)
	{
		GLimp_GrabInput(want_grab);
		have_grab = want_grab;
	}
}

/*
 * Move handling
 */
void
IN_Move(usercmd_t *cmd)
{
	if (m_filter->value)
	{
		if ((mouse_x > 1) || (mouse_x < -1))
		{
			mouse_x = (mouse_x + old_mouse_x) * 0.5;
		}

		if ((mouse_y > 1) || (mouse_y < -1))
		{
			mouse_y = (mouse_y + old_mouse_y) * 0.5;
		}
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if (mouse_x || mouse_y)
	{
		if (!exponential_speedup->value)
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}
		else
		{
			if ((mouse_x > MOUSE_MIN) || (mouse_y > MOUSE_MIN) ||
				(mouse_x < -MOUSE_MIN) || (mouse_y < -MOUSE_MIN))
			{
				mouse_x = (mouse_x * mouse_x * mouse_x) / 4;
				mouse_y = (mouse_y * mouse_y * mouse_y) / 4;

				if (mouse_x > MOUSE_MAX)
				{
					mouse_x = MOUSE_MAX;
				}
				else if (mouse_x < -MOUSE_MAX)
				{
					mouse_x = -MOUSE_MAX;
				}

				if (mouse_y > MOUSE_MAX)
				{
					mouse_y = MOUSE_MAX;
				}
				else if (mouse_y < -MOUSE_MAX)
				{
					mouse_y = -MOUSE_MAX;
				}
			}
		}

		/* add mouse X/Y movement to cmd */
		if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		{
			cmd->sidemove += m_side->value * mouse_x;
		}
		else
		{
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;
		}

		if ((mlooking || freelook->value) && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}

		mouse_x = mouse_y = 0;
	}
}

/* ------------------------------------------------------------------ */

/*
 * Look down
 */
static void
IN_MLookDown(void)
{
	mlooking = true;
}

/*
 * Look up
 */
static void
IN_MLookUp(void)
{
	mlooking = false;
	IN_CenterView();
}

/* ------------------------------------------------------------------ */

/*
 * Initializes the backend
 */
void
IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", 0);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	in_mouse = Cvar_Get("in_mouse", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", 0);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_pitch = Cvar_Get("m_pitch", "0.022", 0);
	m_side = Cvar_Get("m_side", "0.8", 0);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	sensitivity = Cvar_Get("sensitivity", "3", 0);
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	have_grab = GLimp_InputIsGrabbed();

        // TODO orson

#if 0
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetRelativeMouseMode(have_grab ? SDL_TRUE : SDL_FALSE);
	in_relativemode = (SDL_GetRelativeMouseMode() == SDL_TRUE);
	SDL_StartTextInput();
#else
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
#endif
	Com_Printf("------------------------------------\n\n");
}

/*
 * Shuts the backend down
 */
void
IN_Shutdown(void)
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

    Com_Printf("Shutting down input.\n");
}

/* ------------------------------------------------------------------ */

