/**
 * This file is part -2010of Slideshow.
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

#ifndef SLIDESHOW_PATH_H
#define SLIDESHOW_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the real path to a resource.
 * Translates filename into an absolute path in either pkgdatadir (usually
 * /usr/share/slideshow) or in SLIDESHOW_DATA_PATH if that env. variable
 * is set.
 */
char* real_path(const char* filename);

const char* datapath();
const char* pluginpath();

#ifdef __cplusplus
}
#endif

#endif /* SLIDESHOW_PATH_H */
