/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * ChatOnet.h -- represents OnetCzat (http://czat.onet.pl) server.
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

#ifndef CHAT_ONET_H_c3dca5908aed43f251ac818e4cf02817
#	define CHAT_ONET_H_c3dca5908aed43f251ac818e4cf02817

#include "Chat.h"
#include "Tcp.h"
#include "Http.h"

#include <boost/regex.hpp>
#include <boost/bimap.hpp>

class ChatOnet : public Chat, public boost::enable_shared_from_this<ChatOnet>
{
public:
	ChatOnet (Session& session);

	void init ();
	void process (std::vector<std::string>& im);

private:
	void connect ();
	void handle_read ();
	void handle_end (const std::string& reason);
	void handle_connect ();

	void http_handler (int stage, bool error, boost::shared_ptr<std::string> data, boost::shared_ptr<ChatOnet> me);

	static std::string authkey (const std::string& key);

	Tcp tcp_;
	Http http_;
	std::string uokey_;
	size_t sent_bl_notice_;
	boost::regex re_mod_c_, re_mod_f_, re_mod_i_, re_mod_io_, re_mod_ci_;
	std::vector<Channel> chan_list_;
	std::string channel_;
	bool rm_mod_c_, tr_mod_c_, rm_mod_f_, tr_mod_i_;
	boost::bimap<std::string, unsigned int> colors_;
};

#endif
