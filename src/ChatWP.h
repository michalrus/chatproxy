/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * ChatWP.h -- represents WPCzat (http://czat.wp.pl) server.
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

#ifndef CHAT_WP_H_c3dca5908aed43f251ac818e4cf02817
#	define CHAT_WP_H_c3dca5908aed43f251ac818e4cf02817

#include "Chat.h"

#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>

#include "Http.h"
#include "Tcp.h"

class ChatWP : public Chat, public boost::enable_shared_from_this<ChatWP>
{
public:
	ChatWP (Session& session);

	void init ();
	void process (std::vector<std::string>& im);

private:
	void connect ();
	void handle_read ();
	void handle_end (const std::string& reason);
	void handle_connect ();

	void http_handler (int stage, bool error, boost::shared_ptr<std::string> data, boost::shared_ptr<ChatWP> me);

	static std::string decode_nick_full (const std::string& s);
	std::string magic (const std::string& salt);

	Tcp tcp_;
	Http http_;
	size_t sent_bl_notice_;
	bool sent_001_;
	const boost::regex re_avatar_, re_fonts_, re_user_, re_fcuk_;
	std::vector<Channel> chan_list_;
	std::string magic_, nick_, nick_hash_, ticket_;
	boost::filesystem::path img_;
	std::string channel_;
	bool rm_avatar_, rm_fonts_;
	bool captcha_;

	class conv
	{
	public:
		static std::string normal (const std::string& s);
		static std::string hash (const std::string& s);

	private:
		static void init ();

		static bool inited;
		static char q[256], u[256], w[256], A[256], E[256];
		static boost::regex re_hash;
	};
};

#endif
