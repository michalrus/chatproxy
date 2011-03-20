/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Config.h -- chatproxy.conf reading and access.
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

#ifndef CONFIG_H_c3dca5908aed43f251ac818e4cf02817
#	define CONFIG_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/noncopyable.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <string>
#include <vector>
#include <map>

#include "Debug.h"

class Session;

/**
 * $EXEC_DIR/chatproxy.conf parsing.
 *
 * The file contains "<string><whitespace><string>" lines.
 */

struct Config : public boost::noncopyable
{
public:
	Config (const boost::filesystem::path& path);

	/**
	 * File reading procedure.
	 *
	 * Turns "<string><whitespace><string>" lines in path
	 * into a std::map<std::string, std::string>.
	 */
	void read (const boost::filesystem::path& path);

	/**
	 * Returns config from map_ after casting lexically to T
	 * and catches cast/map exceptions.
	 *
	 * This means no config check before use. Sorry. =)
	 */
	template<typename T>
	T get (const std::string& key)
	{
		try {
			std::map<std::string, std::string>::iterator i;
			i = map_.find(key);
			if (i == map_.end())
				throw std::runtime_error("\"" + key + "\" index not found");
			return boost::lexical_cast<T>(i->second);
		}
		catch (...) {
			die ("config file error (while casting \"" + key + "\" directive)");

			/* never happens */
			return T();
		}
	}

	inline void set (const std::string& key, const std::string& value) { map_[key] = value; }

	/**
	 * Vector of sessions. Needed to keep them alive after handle_accept()
	 */
	std::vector<boost::shared_ptr<Session> > sessions_;

	boost::mt19937 rng_;
	boost::filesystem::path path_;

private:
	std::map<std::string, std::string> map_;
};

#endif
