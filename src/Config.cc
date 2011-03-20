/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Config.cc -- chatproxy.conf reading and access.
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

#include "Config.h"

#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>
#include <string>
#include <map>

#include "Debug.h"

Config::Config (const boost::filesystem::path& path)
	: path_(path)
{
	rng_.seed((unsigned int )std::time(0));
}

void Config::read (const boost::filesystem::path& path)
{
	if (!boost::filesystem::is_regular_file(boost::filesystem::status(path)))
		die(path.string() + ": not a file.");

	boost::filesystem::ifstream file(path);
	boost::smatch match;
	std::string line;
	boost::regex re_cmt("^\\s*([#;].*)?$");
	boost::regex re_ent("^\\s*([a-z_]+)\\s+(.+?)\\s*$");

	while (!file.eof()) {
		std::getline(file, line, '\n');

		if (boost::regex_match(line, re_cmt))
			continue;
		else if (boost::regex_match(line, match, re_ent))
			map_[match[1]] = match[2];
		else
			die(path.string() + ": syntax error.");
	}
}
