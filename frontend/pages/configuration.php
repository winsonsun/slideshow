<?
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

class configuration extends Module {
	public function index(){
		global $settings;
		return array(
			'settings' => $settings->as_array(),
			'help' => array(
				'Path' => array(
					'BasePath' => 'Base directory',
					'Image' => 'Image directory',
					'Video' => 'Video directory',
					'Temp' => 'Temp directory'
				),

				'Files' => array(
					'BinaryPath' => 'Location of the slideshow binary.'
				),

				'Database' => array(
					'Password' => 'Leave blank to keep old password',
					'Hostname' => 'hostname[:port]',
					'Name' => 'Name of the database'
				)
			),
			'description' => array(
				'Path' => "The basepath is the homedirectory of the frontend. All other paths are relative to it.",
				'Files' => "All file paths are either relative to BasePath or an absolute path."
			)
		);
	}
}

?>