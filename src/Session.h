/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Session.h -- represents a session between chatproxy3 and IRC client.
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

#ifndef SESSION_H_c3dca5908aed43f251ac818e4cf02817
#	define SESSION_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>

#include "Tcp.h"

class Config;
class Chat;

class Session : public boost::noncopyable
{
public:
	Session (Config& cnf, boost::asio::io_service& io_service);
	~Session ();

	void start ();
	void end (const std::string& reason = "");
	void notice (const std::string& msg);
	void privmsg (const std::string& msg);

	static void brot (std::string& s, int by);
	static std::string conv (const std::string& from, const std::string& to, const std::string& data);
	static std::vector<std::string> parse_im (const std::string& im);
	static std::string build_im (const std::vector<std::string>& im);
	static std::string epoch_to_human (unsigned int epoch, bool only_hms = 0);
	boost::filesystem::path unique (const std::string extension);

	static std::string signature_onl_;
	static std::string signature_app_;

	Tcp tcp_;
	boost::asio::io_service& io_service_;
	std::string chat_nick_, chat_user_, chat_password_, chat_real_, chat_network_;

	Config& cnf_;

private:
	void handle_read ();
	void handle_end (const std::string& reason);

	void auth (std::vector<std::string>& im);
	void adm (const std::string& cmd);

	boost::shared_ptr<Chat> chat_;

	std::string local_host_, local_port_, remote_host_, remote_port_;
	std::string password_;
	bool auth_ok_, password_ok_, nick_ok_, user_ok_;
};

#endif
