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

#include "Chat.h"
#include "Session.h"
#include "Config.h"

Chat::Chat (Session& session)
	: io_service_(session.io_service_), session_(session),
	  charset_(session.cnf_.get<std::string>("charset")),
	  ending_(false)
{
}

Chat::~Chat ()
{
}

void Chat::notice (const std::string& msg)
{
	session_.notice("\00312-\017!\00312-\017 \002" + session_.chat_network_ + "\017: " + msg);
}
