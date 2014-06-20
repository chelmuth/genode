/*
 * \brief  SDL input support
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <SDL/SDL.h>

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <input/keycodes.h>

/* local includes */
#include "input.h"


/**
 * Convert SDL keycode to Genode keycode
 */
static int translate_keysym(SDL_keysym const &keysym)
{
	using namespace Input;

	/*
	 * Translate from X11 keycode to Input::Keycode as X11 and SDL keysyms are
	 * not what we want.
	 */

	switch (keysym.scancode) {

	case   9: return KEY_ESC;

	case  10: return KEY_1;
	case  11: return KEY_2;
	case  12: return KEY_3;
	case  13: return KEY_4;
	case  14: return KEY_5;
	case  15: return KEY_6;
	case  16: return KEY_7;
	case  17: return KEY_8;
	case  18: return KEY_9;
	case  19: return KEY_0;
	case  20: return KEY_MINUS;
	case  21: return KEY_EQUAL;
	case  22: return KEY_BACKSPACE;
	case  23: return KEY_TAB;
	case  24: return KEY_Q;
	case  25: return KEY_W;
	case  26: return KEY_E;
	case  27: return KEY_R;
	case  28: return KEY_T;
	case  29: return KEY_Y;
	case  30: return KEY_U;
	case  31: return KEY_I;
	case  32: return KEY_O;
	case  33: return KEY_P;
	case  34: return KEY_LEFTBRACE;
	case  35: return KEY_RIGHTBRACE;
	case  36: return KEY_ENTER;
	case  37: return KEY_LEFTCTRL;
	case  38: return KEY_A;
	case  39: return KEY_S;
	case  40: return KEY_D;
	case  41: return KEY_F;
	case  42: return KEY_G;
	case  43: return KEY_H;
	case  44: return KEY_J;
	case  45: return KEY_K;
	case  46: return KEY_L;
	case  47: return KEY_SEMICOLON;
	case  48: return KEY_APOSTROPHE;
	case  49: return KEY_GRAVE;
	case  50: return KEY_LEFTSHIFT;
	case  51: return KEY_BACKSLASH;
	case  52: return KEY_Z;
	case  53: return KEY_X;
	case  54: return KEY_C;
	case  55: return KEY_V;
	case  56: return KEY_B;
	case  57: return KEY_N;
	case  58: return KEY_M;
	case  59: return KEY_COMMA;
	case  60: return KEY_DOT;
	case  61: return KEY_SLASH;
	case  62: return KEY_RIGHTSHIFT;
	case  63: return KEY_KPASTERISK;
	case  64: return KEY_LEFTALT;
	case  65: return KEY_SPACE;
	case  66: return KEY_CAPSLOCK;
	case  67: return KEY_F1;
	case  68: return KEY_F2;
	case  69: return KEY_F3;
	case  70: return KEY_F4;
	case  71: return KEY_F5;
	case  72: return KEY_F6;
	case  73: return KEY_F7;
	case  74: return KEY_F8;
	case  75: return KEY_F9;
	case  76: return KEY_F10;
	case  77: return KEY_NUMLOCK;
	case  78: return KEY_SCROLLLOCK;
	case  79: return KEY_KP7;
	case  80: return KEY_KP8;
	case  81: return KEY_KP9;
	case  82: return KEY_KPMINUS;
	case  83: return KEY_KP4;
	case  84: return KEY_KP5;
	case  85: return KEY_KP6;
	case  86: return KEY_KPPLUS;
	case  87: return KEY_KP1;
	case  88: return KEY_KP2;
	case  89: return KEY_KP3;
	case  90: return KEY_KP0;
	case  91: return KEY_KPDOT;

	case  94: return KEY_102ND;
	case  95: return KEY_F11;
	case  96: return KEY_F12;

	case 104: return KEY_KPENTER;
	case 105: return KEY_RIGHTCTRL;
	case 106: return KEY_KPSLASH;

	default:                return 0;
	}
};


static Input::Event wait_for_sdl_event()
{
	using namespace Input;

	SDL_Event event;
	static int mx, my;
	static int ox, oy;

	SDL_WaitEvent(&event);

	/* query new mouse position */
	ox = mx; oy = my;
	if (event.type == SDL_MOUSEMOTION)
		SDL_GetMouseState(&mx, &my);

	/* determine keycode */
	int keycode = 0;
	switch (event.type) {
	case SDL_KEYUP:
	case SDL_KEYDOWN:

		keycode = translate_keysym(event.key.keysym);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:

		switch (event.button.button) {
		case SDL_BUTTON_LEFT:   keycode = BTN_LEFT;   break;
		case SDL_BUTTON_MIDDLE: keycode = BTN_MIDDLE; break;
		case SDL_BUTTON_RIGHT:  keycode = BTN_RIGHT;  break;
		default:                keycode = 0;
		}
	}

	/* determine event type */
	Event::Type type;
	switch (event.type) {
	case SDL_MOUSEMOTION:
		type = Event::MOTION;
		break;

	case SDL_KEYUP:
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_WHEELUP
		 || event.button.button == SDL_BUTTON_WHEELDOWN)
			/* ignore */
			return Event();

		type = Event::RELEASE;
		break;

	case SDL_KEYDOWN:
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_WHEELUP) {
			type = Event::WHEEL;
			return Event(type, 0, 0, 0, 0, 1);
		} else if (event.button.button == SDL_BUTTON_WHEELDOWN) {
			type = Event::WHEEL;
			return Event(type, 0, 0, 0, 0, -1);
		}
		type = Event::PRESS;
		break;

	default:
		return Event();
	}

	return Event(type, keycode, mx, my, mx - ox, my - oy);
}


namespace Input {
	enum { STACK_SIZE = 4096*sizeof(long) };
	struct Backend;
}

struct Input::Backend : Genode::Thread_deprecated<STACK_SIZE>
{
	Handler &handler;

	Backend(Input::Handler &handler)
	:
		Genode::Thread_deprecated<STACK_SIZE>("input_backend"),
		handler(handler)
	{
		start();
	}

	void entry()
	{
		while (true) {
			Input::Event e;

			/* prevent flooding of client with invalid events */
			do {
				e = wait_for_sdl_event();
			} while (e.type() == Input::Event::INVALID);

			handler.event(e);
		}
	}
};


void init_input_backend(Input::Handler &h) { static Input::Backend inst(h); }
