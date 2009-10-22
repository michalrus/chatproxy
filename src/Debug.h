/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Debug.h -- debugging and die/debug helper macros.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEBUG_H_c3dca5908aed43f251ac818e4cf02817
#	define DEBUG_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <string>

/**
 * die() and debug() macros.
 */
#define die(msg)   Debug::_debug(0, __FILE__, __LINE__, __FUNCTION__, msg)
#define debug(msg) Debug::_debug(1, __FILE__, __LINE__, __FUNCTION__, msg)

class Debug
{
public:
	/**
	 * Helper procedure, called by die()
	 * and debug() macros.
	 */
	static void _debug (int type, const std::string& file, unsigned int line,
		const std::string& function, const std::string& msg);

	/**
	 * Opens errlog file.
	 */
	static void open_errlog (const boost::filesystem::path& path);

private:
	static boost::filesystem::fstream errlog_;
	static bool to_screen_;
};

#endif
