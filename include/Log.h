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

#ifndef LOG_H
#define LOG_H

#include <cstdio>

class Log {
	public:
		static void initialize(const char* filename, const char* debugfilename);
		static void deinitialize();

		enum Severity {
			Debug,
			Verbose,
			Warning,
			Fatal
		};

		static void message(Severity severity, const char* fmt, ...);

	private:
		Log(){}

		static char *timestring(char *buffer, int bufferlen);
		static const char* severity_string(Severity severity);

		static FILE* _file;
		static FILE* _dfile;
};

#endif // LOG_H
