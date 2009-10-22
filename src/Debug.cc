/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Debug.cc -- debugging and die/debug helper macros.
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

#include "Debug.h"

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

#include "Session.h"

void Debug::_debug (int type, const std::string& file, unsigned int line,
	const std::string& function, const std::string& msg)
{
	std::string entry(boost::filesystem::path(file).filename() + ":"
		+ boost::lexical_cast<std::string>(line) + " in " + function
		+ "(): " + (type ? "[debug] " : "[FATAL] ") + msg);

	if (to_screen_) {
#ifndef BOOST_WINDOWS
		std::cerr << entry << std::endl;
#else
		MessageBox(NULL, entry.c_str(), "chatproxy3", MB_OK);
#endif
	}
	else {
		entry.insert(0, "[" + Session::epoch_to_human(std::time(NULL)) + "] ");
		entry += "\n";
		errlog_.write(entry.data(), entry.length());
	}

	if (!type)
		std::exit(-1);
}

void Debug::open_errlog (const boost::filesystem::path& path)
{
	errlog_.open(path, std::ios::out | std::ios::app);
	if (errlog_.fail())
		throw std::runtime_error("could not open " + path.string());
	to_screen_ = false;
}

boost::filesystem::fstream Debug::errlog_;
bool Debug::to_screen_(true);
