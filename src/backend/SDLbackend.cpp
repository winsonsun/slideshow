#include "backend/SDLbackend.h"
#include "Exceptions.h"

#ifdef WIN32
#	include "win32.h"
#endif

static const int sdl_flags = SDL_OPENGL | SDL_DOUBLEBUF;
static PlatformBackend* factory(void){
	return new SDLBackend;
}

void SDLBackend::register_factory(){
	PlatformBackend::register_factory("sdl", ::factory);
}

SDLBackend::SDLBackend()
	: PlatformBackend()
	, _lock(false) {

}

SDLBackend::~SDLBackend(){

}

int SDLBackend::init(const Vector2ui &resolution, bool fullscreen){
	set_resolution(resolution.width, resolution.height);

	int flags = sdl_flags;

	if ( fullscreen ){
		flags |= SDL_FULLSCREEN;
	}

	_fullscreen = fullscreen;
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(resolution.width, resolution.height, 0, flags);
	SDL_EnableUNICODE(1);

#ifdef WIN32
	SetConsoleOutputCP(65001);
#endif

	return 0;
}

void SDLBackend::cleanup(){
	SDL_Quit();
}

void SDLBackend::poll(bool& running){
	SDL_Event event;
		while(SDL_PollEvent(&event) ){
			switch(event.type){
				case SDL_KEYDOWN:
					if ( event.key.keysym.sym == SDLK_ESCAPE ){
						running = false;
					}

					if ( (event.key.keysym.sym == SDLK_RETURN) && (event.key.keysym.mod & KMOD_ALT)){
						_fullscreen = !_fullscreen;
						int flags = sdl_flags;

						if ( _fullscreen ){
							flags |= SDL_FULLSCREEN;
						}

						
						SDL_SetVideoMode(resolution().width, resolution().height, 0, flags);
					}

					break;

				case SDL_VIDEORESIZE:
					printf("video resize\n");
					set_resolution(event.resize.w, event.resize.h);
					break;

				case SDL_QUIT:
					running = false;
					break;
			}
		}
}

void SDLBackend::swap_buffers() const {
	SDL_GL_SwapBuffers();
}

void SDLBackend::lock_mouse(bool state){
	_lock = state;

	if ( _lock ){
		SDL_WarpMouse(center().x, center().y);
	}
}
