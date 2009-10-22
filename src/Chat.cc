#include "Chat.h"

#include "Session.h"
#include "Config.h"

Chat::Chat (Session* session)
	: io_service_(session->io_service_),
	  nick_(session->nick_), username_(session->username_), password_(session->password_),
	  realname_(session->realname_), session_(session)
{
}

void Chat::end (const std::string& reason)
{
	session_->end(reason);
}

void Chat::send (const std::vector<std::string>& im, bool lastText)
{
	session_->send(im, lastText);
}

void Chat::sendInfo (const std::string& msg, const std::string& to)
{
	session_->sendInfo(msg, to);
}

void Chat::sendRaw (const std::string& data)
{
	session_->sendRaw(data);
}

std::string Chat::epochToHuman (unsigned int epoch, bool onlyHMS)
{
	return Session::epochToHuman(epoch, onlyHMS);
}

boost::tuple<boost::filesystem::path, std::string> Chat::genToken ()
{
	return session_->cnf_.genToken();
}

bool Chat::adm () const
{
	return session_->user_->adm;
}
