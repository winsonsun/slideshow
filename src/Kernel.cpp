/**
 * This file is part of Slideshow.
 * Copyright (C) 2008-2010 David Sveningsson <ext@sidvind.com>
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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "argument_parser.h"
#include "module.h"
#include "module_loader.h"
#include "Kernel.h"
#include "Graphics.h"
#include "OS.h"
#include "path.h"
#include "Log.h"
#include "exception.h"
#include "Transition.h"

// IPC
#ifdef HAVE_DBUS
#	include "IPC/dbus.h"
#endif /* HAVE_DBUS */

// FSM
#include "state/State.h"
#include "state/InitialState.h"
#include "state/SwitchState.h"
#include "state/TransitionState.h"
#include "state/ViewState.h"

// Backend
#include "backend/platform.h"

// libportable
#include <portable/Time.h>
#include <portable/asprintf.h>
#include <portable/scandir.h>
#include <portable/cwd.h>

// libc
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

// Platform
#ifdef __GNUC__
#	include <sys/wait.h>
#endif

#ifdef WIN32
#	include "win32.h"
#	include <direct.h>
#	define getcwd _getcwd
#	define PATH_MAX _MAX_PATH
#endif

static char* pidfile = NULL;

Kernel::Kernel(const argument_set_t& arg, PlatformBackend* backend)
	: _arg(arg)
	, _password(NULL)
	, _state(NULL)
	, _graphics(NULL)
	, _browser(NULL)
	, _ipc(NULL)
	, _backend(backend)
	, _running(false) {

	verify(_backend);

	create_pidpath();
	_password = get_password();
}

Kernel::~Kernel(){
	delete _backend;

	free( _arg.connection_string );
	free( _arg.transition_string );
}

void Kernel::init(){
	Log::message(Log_Info, "Kernel: Starting slideshow\n");

	init_backend();
	init_graphics();
	init_IPC();
	init_browser();
	init_fsm();
}

void Kernel::cleanup(){
	delete _state;
	module_close(&_browser->module);
	delete _graphics;
	delete _ipc;
	free(pidfile);
	free(_password);

	cleanup_backend();

	_state = NULL;
	_browser = NULL;
	_graphics = NULL;
	_ipc = NULL;
	pidfile = NULL;
}

void Kernel::init_backend(){
	_backend->init(Vector2ui(_arg.width, _arg.height), _arg.fullscreen > 0);
}

void Kernel::cleanup_backend(){
	_backend->cleanup();
}

void Kernel::init_graphics(){
	_graphics = new Graphics(_arg.width, _arg.height, _arg.fullscreen > 0);
	load_transition( _arg.transition_string ? _arg.transition_string : "fade" );
}

void Kernel::init_IPC(){
#ifdef HAVE_DBUS
	_ipc = new DBus(this, 50);
#endif /* HAVE_DBUS */
}

void Kernel::init_browser(){
	browser_context_t context = get_context(_arg.connection_string);

	// If the contex doesn't contain a password and a password was passed from stdin (arg password)
	// we set that as the password in the context.
	if ( !context.pass && _password ){
		context.pass = strdup(_password);
	}

	_browser = (struct browser_module_t*)module_open(context.provider, BROWSER_MODULE, MODULE_CALLER_INIT);

	if ( !_browser ){
		Log::message(Log_Warning, "Failed to load browser plugin '%s': %s\n", context.provider, module_error_string());
		Log::message(Log_Warning, "No browser selected, you will not see any slides\n");
		return;
	}

	_browser->context = context;
	_browser->module.init((module_handle)_browser);

	change_bin(_arg.collection_id);
}

char* Kernel::get_password(){
	if ( !_arg.have_password ){
		return NULL;
	}

	char* password = (char*)malloc(256);
	verify( scanf("%256s", password) == 1 );

	return password;
}

void Kernel::init_fsm(){
	TransitionState::set_transition_time(_arg.transition_time);
	ViewState::set_view_time(_arg.switch_time);
	_state = new InitialState(_browser, _graphics, _ipc);
}

void Kernel::load_transition(const char* name){
	_graphics->set_transition(name);
}

void Kernel::poll(){
	_backend->poll(_running);
}

void Kernel::action(){
	if ( !_state ){
		return;
	}

	bool flip = false;

	try {
		_state = _state->action(flip);
	} catch ( exception& e ){
		Log::message(Log_Warning, "State exception: %s\n", e.what());
		_state = NULL;
	}

	if ( flip ){
		_backend->swap_buffers();
	}
}

void Kernel::print_config() const {
	char* cwd = get_current_dir_name();

	Log::message(Log_Info, "Slideshow configuration\n");
	Log::message(Log_Info, "  cwd: %s\n", cwd);
	Log::message(Log_Info, "  pidfile: %s\n", pidfile);
	Log::message(Log_Info, "  datapath: %s\n", datapath());
	Log::message(Log_Info, "  pluginpath: %s\n", pluginpath());
	Log::message(Log_Info, "  resolution: %dx%d (%s)\n", _arg.width, _arg.height, _arg.fullscreen ? "fullscreen" : "windowed");
	Log::message(Log_Info, "  transition time: %0.3fs\n", _arg.transition_time);
	Log::message(Log_Info, "  switch time: %0.3fs\n", _arg.switch_time);
	Log::message(Log_Info, "  connection string: %s\n", _arg.connection_string);
	Log::message(Log_Info, "  transition: %s\n", _arg.transition_string);
	Log::message(Log_Info, "\n");

	free(cwd);
}

void Kernel::print_licence_statement() const {
	Log::message(Log_Info, "Slideshow  Copyright (C) 2008-2010 David Sveningsson <ext@sidvind.com>\n");
	Log::message(Log_Info, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	Log::message(Log_Info, "This is free software, and you are welcome to redistribute it\n");
	Log::message(Log_Info, "under certain conditions; see COPYING or <http://www.gnu.org/licenses/>\n");
	Log::message(Log_Info, "for details.\n");
	Log::message(Log_Info, "\n");
}

#ifdef WIN32
#	define SO_SUFFIX ".dll"
#else
#	define SO_SUFFIX ".la"
#endif

static int filter(const struct dirent* el){
	return fnmatch("*" SO_SUFFIX , el->d_name, 0) == 0;
}

void Kernel::print_transitions(){
	Log::message(Log_Info, "Available transitions: \n");

	struct dirent **namelist;
	int n;

	char* path_list = strdup(pluginpath());
	char* ctx;
	char* path = strtok_r(path_list, ":", &ctx);
	while ( path ){
		n = scandir(path, &namelist, filter, NULL);
		if (n < 0){
			perror("scandir");
		} else {
			for ( int i = 0; i < n; i++ ){
				module_handle context = module_open(namelist[i]->d_name, ANY_MODULE, MODULE_CALLEE_INIT);
				free(namelist[i]);

				if ( !context ){
					continue;
				}

				if ( module_type(context) != TRANSITION_MODULE ){
					continue;
				}

				Log::message(Log_Info, " * %s\n", module_get_name(context));

				module_close(context);
			}
			free(namelist);
		}

		path = strtok_r(NULL, ":", &ctx);
	}

	free(path_list);
}

bool Kernel::parse_arguments(argument_set_t& arg, int argc, const char* argv[]){
	option_set_t options;

	option_initialize(&options, argc, argv);
	option_set_description(&options, "Slideshow is an application for showing text and images in a loop on monitors and projectors.");

	option_add_flag(&options,	"verbose",			'v', "Include debugging messages in log.", &arg.loglevel, Log_Debug);
	option_add_flag(&options,	"quiet",			'q', "Show only warnings and errors in log.", &arg.loglevel, Log_Warning);
	option_add_flag(&options,	"fullscreen",		'f', "Start in fullscreen mode", &arg.fullscreen, true);
	option_add_flag(&options,	"window",			'w', "Start in windowed mode [default]", &arg.fullscreen, false);
	option_add_flag(&options,	"daemon",			'd', "Run in background", &arg.mode, DaemonMode);
	option_add_flag(&options,	"list-transitions",	 0,  "List available transitions", &arg.mode, ListTransitionMode);
	option_add_flag(&options,	"stdin-password",	 0,  "Except the input (e.g database password) to come from stdin", &arg.have_password, true);
	option_add_string(&options,	"browser",			 0,  "Browser connection string. provider://user[:pass]@host[:port]/name", &arg.connection_string);
	option_add_string(&options,	"transition",		't', "Set slide transition plugin [fade]", &arg.transition_string);
	option_add_int(&options,	"collection-id",	'c', "ID of the collection to display", &arg.collection_id);
	option_add_format(&options,	"resolution",		'r', "Resolution", "WIDTHxHEIGHT", "%dx%d", &arg.width, &arg.height);

	/* logging options */
	option_add_string(&options,	"file-log",			 0,  "Log to regular file (appending)", &arg.log_file);
	option_add_string(&options,	"fifo-log",			 0,  "Log to a named pipe", &arg.log_fifo);
	option_add_string(&options,	"uds-log",			 0,  "Log to a unix domain socket", &arg.log_domain);

	int n = option_parse(&options);
	option_finalize(&options);

	if ( n < 0 ){
		return false;
	}

	if ( n != argc ){
		printf("%d %d\n", n, argc);
		printf("%s: unrecognized option '%s'\n", argv[0], argv[n+1]);
		printf("Try `%s --help' for more information.\n", argv[0]);
		return false;
	}

	return true;
}

void Kernel::play_video(const char* fullpath){
#ifndef WIN32
	Log::message(Log_Info, "Kernel: Playing video \"%s\"\n", fullpath);

	int status;

	if ( fork() == 0 ){
		execlp("mplayer", "", "-fs", "-really-quiet", fullpath, NULL);
		exit(0);
	}

	::wait(&status);
#else /* WIN32 */
	Log::message(Log_Warning, "Kernel: Video playback is not supported on this platform (skipping \"%s\")\n", fullpath);
#endif /* WIN32 */
}

void Kernel::start(){
	_running = true;
}

void Kernel::quit(){
	_running = false;
}

void Kernel::reload_browser(){
	_browser->queue_reload(_browser);
}

void Kernel::change_bin(unsigned int id){
	Log::message(Log_Verbose, "Kernel: Switching to queue %d\n", id);
	_browser->queue_set(_browser, id);
	_browser->queue_reload(_browser);
}

void Kernel::ipc_quit(){
	delete _ipc;
	_ipc = NULL;
}

void Kernel::debug_dumpqueue(){
	_browser->queue_dump(_browser);
}

void Kernel::create_pidpath(){
	char* cwd = get_current_dir_name();
	verify( asprintf(&pidfile, "%s/slideshow.pid", cwd) >= 0 );
	free(cwd);
}

const char* Kernel::pidpath(){
	return pidfile;
}
