/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Chat.h -- represents an abstract session between chatproxy3 and
 * chat server.
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

#ifndef CHAT_H_c3dca5908aed43f251ac818e4cf02817
#	define CHAT_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <vector>
#include <string>

class Session;

class Chat : public boost::noncopyable {
public:
	/**
	 * Represents a channel on a chat server.
	 * Used to display a sorted channel list after "LIST\r\n".
	 */
	struct Channel {
		std::string name;
		size_t num;
		std::string topic;

		bool operator< (const Channel& rhs) const {
			if (num == rhs.num)
				return name < rhs.name;
			return num < rhs.num;
		}
	};

	Chat (Session& session);
	virtual ~Chat ();

	virtual void init() = 0;
	virtual void process(std::vector<std::string>& im) = 0;

	inline void end () { ending_ = true ; }

protected:
	void notice (const std::string& msg);

	/**
	 * Generates unique file name used to store CAPTCHA
	 * images.
	 */
	std::string gen_uuid () const;

	boost::asio::io_service& io_service_;
	Session& session_;
	std::string charset_;

	bool ending_;
};

#endif
