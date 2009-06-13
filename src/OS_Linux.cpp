/**
 * This file is part of Slideshow.
 * Copyright (C) 2008 David Sveningsson <ext@sidvind.com>
 *
 * Slideshow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Slideshow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Slideshow.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "OS.h"
#include "Log.h"
//#include <cstdio>
#include "Exceptions.h"
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#if HAVE_XRANDR
#	include <X11/extensions/Xrandr.h>
#endif /* HAVE_XRANDR */

#if HAVE_XF86VIDMODE
#	include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VIDMODE */

#include <GL/glx.h>

typedef struct __glx_state {
	Display* dpy;
	Window win;
	Window root;
	GLXContext ctx;
	GLXDrawable glx_drawable;
	int width;
	int height;
	bool in_fullscreen;
	bool fullscreen_available;
#if HAVE_XRANDR
	XRRScreenConfiguration* screen_config;
	int size_id;
#endif
} glx_state;

/* @todo Remove usage of this global state */
static glx_state g;

#if HAVE_XRANDR
XRRScreenConfiguration* saved_screen_config = NULL;
Rotation saved_rotation;
int saved_size_id = -1;
#endif

#if HAVE_XF86VIDMODE
static XF86VidModeModeInfo **vidmodes;
#endif /* HAVE_XF86VIDMODE */

static Atom wm_delete_window;
static Atom wm_fullscreen;
static Atom wm_state;

Cursor default_cursor = 0;
Cursor no_cursor = 0;

enum {
	_NET_WM_STATE_REMOVE =0,
	_NET_WM_STATE_ADD = 1,
	_NET_WM_STATE_TOGGLE =2
};

/**
 * Generates X interal atoms.
 * @param dpy Specifies the connection to the X server.
 */
void generate_atoms(Display* dpy){
	wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm_fullscreen = XInternAtom(dpy,"_NET_WM_STATE_FULLSCREEN", False);
	wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
}

void generate_cursors(Display* dpy, Window win){
	Pixmap blank;
	XColor dummy;
	char data[1] = {0};

	blank = XCreateBitmapFromData (dpy, win, data, 1, 1);
	no_cursor = XCreatePixmapCursor(dpy, blank, blank, &dummy, &dummy, 0, 0);
	XFreePixmap (dpy, blank);
	default_cursor = XCreateFontCursor(dpy, XC_left_ptr);
}

void set_cursor(glx_state* state, Cursor cursor){
	XDefineCursor(state->dpy, state->win, cursor);
}

int doubleBufferAttributes[] = {
	GLX_DRAWABLE_TYPE,	GLX_WINDOW_BIT,
	GLX_RENDER_TYPE,	GLX_RGBA_BIT,
	GLX_DOUBLEBUFFER,	True,
	None
};

/**
 * Return a glXVisualInfo from a FBConfig attribute list.
 * @param dpy Specifies the connection to the X server.
 * @param screen  Specifies the screen number.
 * @param attribList Specifies a list of boolean attributes and integer attribute/value pairs.  The last attribute must be None.
 */
XVisualInfo* glXVisualFromFBConfigAttributes(Display* dpy, int screen, int* attribList){
	int configs = 0;

	GLXFBConfig* fbConfigs = glXChooseFBConfig(dpy, screen, attribList, &configs);
	if ( !fbConfigs ) {
		throw XlibException( "No double buffered config available\n" );
	}

	XVisualInfo* visual = glXGetVisualFromFBConfig(dpy, fbConfigs[0]);
	if ( !visual ){
		throw XlibException("No appropriate visual found\n");
	}

	return visual;
}

void store_display_config(Display* dpy, Window root){
#if HAVE_XRANDR
	saved_screen_config = XRRGetScreenInfo(dpy, root);
	saved_size_id = XRRConfigCurrentConfiguration(saved_screen_config, &saved_rotation);
#elif HAVE_XF86VIDMODE

#endif
}

enum fullscreen_state_t {
	ENABLE = _NET_WM_STATE_ADD,
	DISABLE = _NET_WM_STATE_REMOVE,
	TOGGLE = _NET_WM_STATE_TOGGLE
};

/**
 * Checks if the specified resolution is available. It is important that the
 * exact resolution requested is used since the slides has been created in that
 * resolution. No scaling must occur.
 *
 * @param dpy Specifies the connection to the X server.
 * @param root Specifies the X window.
 * @param width
 * @param height
 */
bool resolution_available(Display* dpy, Window root, int width, int height){
#if HAVE_XRANDR
	XRRScreenConfiguration* screen_config = XRRGetScreenInfo(dpy, root);
	g.screen_config = screen_config;

	int nsizes;
	XRRScreenSize *sizes = XRRConfigSizes(screen_config, &nsizes);

	for ( int i = 0; i < nsizes; i++ ){
		if ( width == sizes[i].width && height == sizes[i].height ){
			g.size_id = i;
			return true;
		}
	}
#elif HAVE_XF86VIDMODE
#	error NOT IMPLEMENTED
#endif
	return false;
}

void enter_fullscreen(glx_state* state){
#if HAVE_XRANDR
	XRRSetScreenConfig(state->dpy, state->screen_config, state->root, state->size_id, saved_rotation, CurrentTime);
#elif HAVE_XF86VIDMODE
#	error NOT IMPLEMENTED
#endif
	state->in_fullscreen = true;
}

void exit_fullscreen(glx_state* state){
#if HAVE_XRANDR
	XRRSetScreenConfig(state->dpy, saved_screen_config, state->root, saved_size_id, saved_rotation, CurrentTime);
#elif HAVE_XF86VIDMODE
#	error NOT IMPLEMENTED
#endif
	state->in_fullscreen = false;
}

/**
 * Enable, disable or toggle fullscreen mode.
 * @param dpy Specifies the connection to the X server.
 * @param win Specifies the X window.
 * @param status Action to perform.
 */
void set_fullscreen(glx_state* state, fullscreen_state_t status){
	if ( !state->fullscreen_available ){
		Log::message(Log::Warning, "Graphics: Cannot enter fullscreen mode as the requested resolution %dx%d isn't available.\n", state->width, state->height);
		return;
	}

	switch ( status ) {
		case ENABLE: enter_fullscreen(state); break;
		case DISABLE: exit_fullscreen(state); break;
		case TOGGLE:
			if ( state->in_fullscreen ){
				exit_fullscreen(state);
			} else {
				enter_fullscreen(state);
			}
			break;
	}

	/* Notify the window manager to enable/disable the window decorations */
	XEvent xev;

	xev.xclient.type =         ClientMessage;
	xev.xclient.serial =       0;
	xev.xclient.send_event =   True;
	xev.xclient.window =       state->win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format =       32;

	xev.xclient.data.l[0] = status;
	xev.xclient.data.l[1] = wm_fullscreen;
	xev.xclient.data.l[2] = 0;

	XSendEvent(state->dpy, state->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

bool is_window_delete_event(XEvent* event){
	return (unsigned int)event->xclient.data.l[0] == wm_delete_window;
}

void OS::init_view(int width, int height, bool fullscreen){
	Display* dpy = XOpenDisplay(NULL);

	if( !dpy ) {
		throw XlibException("Could not connect to an X server");
	}

	Window root = DefaultRootWindow(dpy);
	XVisualInfo* vi = glXVisualFromFBConfigAttributes(dpy, DefaultScreen(dpy), doubleBufferAttributes);

	bool fullscreen_available = resolution_available(dpy, root, width, height);
	if ( fullscreen && !fullscreen_available ){
		throw XlibException("The specified resolution %dx%d is not available in fullscreen mode", width, height);
	}

	store_display_config(dpy, root);

	unsigned long mask = CWColormap | CWEventMask;
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask ;

	if ( fullscreen ){
		swa.override_redirect = True;
		swa.backing_store = NotUseful;
		swa.save_under = False;
	}

	Window win = XCreateWindow(dpy, root, 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, mask, &swa);

	XStoreName(dpy, win, "Slideshow");

	XMapWindow(dpy, win);

	GLXContext ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, ctx);
	GLXDrawable glx_drawable = glXGetCurrentDrawable();

	g.dpy = dpy;
	g.win = win;
	g.root = root;
	g.ctx = ctx;
	g.glx_drawable = glx_drawable;
	g.width = width;
	g.height = height;
	g.in_fullscreen = false;
	g.fullscreen_available = fullscreen_available;

	generate_atoms(dpy);

	XSetWMProtocols(dpy, win, &wm_delete_window, 1);

	/* make a blank cursor */
	generate_cursors(dpy, win);
	set_cursor(&g, no_cursor);

	if ( fullscreen ){
		Log::message(Log::Verbose, "Graphics: Going fullscreen\n");
		set_fullscreen(&g, ENABLE);
	}
}

void OS::swap_gl_buffers(){
	glXSwapBuffers(g.dpy, g.glx_drawable);
}

void OS::cleanup(){
	if ( g.in_fullscreen ){
		exit_fullscreen(&g);
	}

	set_cursor(&g, default_cursor);
  glXDestroyContext(g.dpy, g.ctx);

  if( g.win && g.dpy ){
	XDestroyWindow(g.dpy, g.win );
	g.win = (Window) 0;
  }

  if ( g.dpy ){
	XCloseDisplay( g.dpy );
	g.dpy = 0;
  }
}

void OS::poll_events(bool& running){
	XEvent event;

	while ( XPending(g.dpy) > 0 ){
		XNextEvent(g.dpy, &event);
		switch (event.type){
			case KeyPress:
				if ( event.xkey.state == 24 && event.xkey.keycode == 36 ){
					set_fullscreen(&g, TOGGLE);
					continue;
				}

				if ( event.xkey.keycode == 9 ){
					running = false;

					continue;
				}

				break;

			case ClientMessage:
				if( is_window_delete_event(&event) ){
					running = false;
					continue;
				}
				break;
		}
	}
}
