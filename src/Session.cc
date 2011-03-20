/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Session.cc -- represents a session between chatproxy3 and IRC client.
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

#include "Session.h"

#include <boost/shared_array.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/random.hpp>
#include <sstream>
#include <ctime>
#include <iconv.h>

#include "Debug.h"
#include "Config.h"
#include "ChatOnet.h"
#include "ChatWP.h"
/* #include "ChatInteria.h" */

Session::Session (Config& cnf, boost::asio::io_service& io_service)
	: tcp_(io_service,
			boost::bind(&Session::handle_read, this),
			boost::bind(&Session::handle_end, this, _1),
			boost::function<void()>()),
	  io_service_(io_service),
	  cnf_(cnf),
	  password_(cnf_.get<std::string>("password")),
	  auth_ok_(false), password_ok_(false), nick_ok_(false), user_ok_(false)
{
}

Session::~Session ()
{
}

void Session::start()
{
	local_host_ = tcp_.socket().local_endpoint().address().to_string();
	local_port_ = boost::lexical_cast<std::string>(tcp_.socket().local_endpoint().port());
	remote_host_ = tcp_.socket().remote_endpoint().address().to_string();
	remote_port_ = boost::lexical_cast<std::string>(tcp_.socket().remote_endpoint().port());

	tcp_.timeout(5);
	tcp_.read_until("\n");
	tcp_.write(":chatproxy3 020 * :Please wait while we process your connection.\r\n");
}

void Session::end (const std::string& reason)
{
	if (!reason.empty()) {
		notice("");
		notice("\002\0034ERROR\017\002: \017" + reason);
		notice("");
	}

	tcp_.end();
}

void Session::notice (const std::string& msg)
{
	tcp_.write(":chatproxy3 NOTICE * :" + msg + "\r\n");
}

void Session::privmsg (const std::string& msg)
{
	tcp_.write(":&chatproxy3 PRIVMSG " + chat_nick_ + " :" + msg + "\r\n");
}

/**
 * Binary rot. Prevents naive editing of binary executable. ;)
 */
void Session::brot (std::string& s, int by)
{
	for (size_t i = 0; i < s.length(); i++)
		s[i] = (char )(((int )(unsigned char )s[i] + by) % 256);
}

std::string Session::conv (const std::string& from, const std::string& to, const std::string& data)
{
	iconv_t cd = iconv_open(to.c_str(), from.c_str());
	if (cd == (iconv_t )-1) {
		debug("iconv_open error");
		/* iconv_open error */
		return data;
	}

	size_t inbytesleft = data.length(), outbytesleft = data.length() * 2;

	char  buf[outbytesleft];
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)
	char inbuf[inbytesleft + 1];
	std::copy(data.begin(), data.end(), inbuf);
	inbuf[inbytesleft] = '\0';
	char* inptr = inbuf;
#else
	const char* inptr = data.c_str();
#endif
	char* outptr = buf;

	size_t nconv = iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);

	iconv_close(cd);

	if (nconv == (size_t )-1) {
		/* iconv error */
		debug("iconv error (\"" + data + "\")");
		return data;
	}
	else
		return std::string(buf, data.length() * 2 - outbytesleft);
}

std::vector<std::string> Session::parse_im (const std::string& im)
{
	std::vector<std::string> r;
	size_t begin = 0;
	bool begin_set = false;

	for (size_t i = 0; i < im.length() + 1; i++)
		if (i == im.length() || im[i] == ' ' || im[i] == '\t' || im[i] == '\n' || im[i] == '\r') {
			if (begin_set) {
				r.push_back(im.substr(begin, i - begin));
				begin_set = false;
			}
		}
		else {
			if (!begin_set) {
				if (im[i] == ':') {
					if (r.size()) {
						size_t j = 1;
						while (im[im.length() - j] == '\r' || im[im.length() - j] == '\n')
							j++;
						r.push_back(im.substr(i + 1, im.length() - (i + 1) - (j - 1)));
						break;
					}
				}
				begin = i;
				begin_set = true;
			}
		}

	return r;
}

std::string Session::build_im (const std::vector<std::string>& im)
{
	std::string r;

	for (size_t i = 0; i < im.size(); i++) {
		if (i)
			r += ' ';
		if (i == im.size() - 1 && (0
			|| !im[i].length()
			|| (im.size() == 4 && (im[1] == "PRIVMSG" || im[1] == "NOTICE"))
			|| (im.size() == 3 && (im[0] == "PRIVMSG" || im[0] == "NOTICE"))
			|| im[i].find(' ') != std::string::npos
			|| im[i].find('\t') != std::string::npos
			))
			r += ':';
		r += im[i];
	}

	r += "\r\n";

	return r;
}

std::string Session::epoch_to_human (unsigned int epoch, bool only_hms)
{
	time_t tme = epoch;
	struct tm* tv;
	tv = localtime(&tme);
	std::stringstream stme;
	stme << std::setfill('0');
	if (!only_hms)
		stme << std::setw(4) << (1900 + tv->tm_year) << std::setw(2) << (tv->tm_mon + 1) << std::setw(2) << tv->tm_mday << ".";
	stme << std::setw(2) << tv->tm_hour << std::setw(2) << tv->tm_min << std::setw(2) << tv->tm_sec;
	return stme.str();
}

boost::filesystem::path Session::unique (const std::string extension)
{
	const static std::string alph("0123456789abcdefghijklmnopqrstuvwxyz");

	boost::uniform_int<unsigned int> u(0, alph.length() - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<unsigned int> > gen(cnf_.rng_, u);
	std::string id;

	do {
		id.clear();
		for (size_t i = 0; i < 8; i++)
			id += alph[gen()];
		id += "." + extension;
	} while (boost::filesystem::is_regular_file(boost::filesystem::status(cnf_.path_ / id)));

	return cnf_.path_ / id;
}

std::string Session::signature_onl_("\x73\xd4\xd9\xd2\xe5\xe1\xe3\xe0\xe9\xea\xa4\x73\x91\xa0\x91\x73\xde\xda\xd4\xd9\xd2\xdd\xe3\xe6\xe4\x9f\xd4\xe0\xde\x73");
std::string Session::signature_app_("\x80\x91\x99" + Session::signature_onl_ + "\x9a");

void Session::handle_read ()
{
	std::vector<std::string> im(parse_im(tcp_.data()));

	if (!im.empty()) {
		boost::to_upper(im[0]);

		if (!auth_ok_)
			auth(im);
		else if (im.size() == 3 && im[0] == "PRIVMSG" && im[1] == "&chatproxy3")
			adm(im[2]);
		else if (chat_ && !im.empty())
			chat_->process(im);
		/* else: we're finished */
	}

	tcp_.read_until("\n");
}

void Session::handle_end (const std::string& reason)
{
	/*
	 * if (!reason.empty())
	 *	debug(reason);
	 */

	if (chat_)
		chat_->end();

	/* destroy *this => remove reference to boost::shared_ptr<Session>(this)
	 * from Config::sessions vector
	 */
	for (size_t i = 0; i < cnf_.sessions_.size(); i++)
		if (cnf_.sessions_[i].get() == this) {
			/* this is this =) */
			cnf_.sessions_.erase(cnf_.sessions_.begin() + i);
			return;
		}
}

void Session::auth (std::vector<std::string>& im)
{
	try {
		if (im[0] == "CAP") {
			/* ignore mIRC fiddlesticks */
		}
		else if (im.size() < 2)
			throw std::runtime_error("\002unknown command: \017" + im[0] + "\002 (or it takes some args)");
		else if (im[0] == "PASS") {
			if (im[1] != password_)
				throw std::runtime_error("\002access denied; use \017<pw>\002 from \017chatproxy3.conf");
			password_ok_ = true;
		}
		else if (im[0] == "NICK") {
			size_t p;

			if ((p = im[1].find('@')) == std::string::npos)
				throw std::runtime_error("\002could not find '@' in your NICK (\017" + im[1] + "\002)");

			chat_nick_ = im[1].substr(0, p);
			chat_network_ = im[1].substr(p + 1);
			boost::to_lower(chat_network_);

			if ((p = chat_nick_.find(':')) != std::string::npos) {
				chat_password_ = chat_nick_.substr(p + 1);
				chat_nick_ = chat_nick_.substr(0, p);

				if ((p = chat_password_.find(':')) != std::string::npos) {
					chat_user_ = chat_password_.substr(0, p);
					chat_password_ = chat_password_.substr(p + 1);
				}
			}
			nick_ok_ = true;
		}
		else if (im[0] == "USER") {
			if (im.size() == 5)
				chat_real_ = im[4];
			else
				throw std::runtime_error("\002command USER takes 4 arguments");
			user_ok_ = 1;
		}
		else
			throw std::runtime_error("\002unknown command: \017" + im[0]);

		if (user_ok_ && nick_ok_ && !password_ok_)
			throw std::runtime_error("\002no PASS command sent");

		if (!user_ok_ || !nick_ok_ || !password_ok_)
			/* wait for all three commands */
			return;

		if (chat_network_ == "onet")
			chat_ = boost::shared_ptr<Chat>(new ChatOnet(*this));
		else if (chat_network_ == "wp")
			chat_ = boost::shared_ptr<Chat>(new ChatWP(*this));
		/*
		 * else if (chat_network_ == "interia")
		 *	chat_ = boost::shared_ptr<Chat>(new ChatInteria(this));
		 */

		if (!chat_)
			throw std::runtime_error("\002unsupported network (\017" + chat_network_ + "\002)");
	}
	catch (std::runtime_error& e) {
		notice("");
		notice("\002chatproxy3 v3.0");
		notice("\002Copyright (C) 2011  Micha"
			+ conv("UTF-8", cnf_.get<std::string>("charset"), "\xc5\x82")
			+ " Rus");
		notice("\002http://michalrus.com/code/chatproxy3/");
		notice("");
		notice("This program is free software: you can redistribute it");
		notice("and/or modify it under the terms of the GNU General");
		notice("Public License as published by the Free Software");
		notice("Foundation, either version 3 of the License, or (at");
		notice("your option) any later version.");
		notice("");
		notice("This program is distributed in the hope that it will be");
		notice("useful, but WITHOUT ANY WARRANTY; without even the");
		notice("implied warranty of MERCHANTABILITY or FITNESS FOR A");
		notice("PARTICULAR PURPOSE.  See the GNU General Public License");
		notice("for more details.");
		notice("");
		notice("You should have received a copy of the GNU General");
		notice("Public License along with this program.  If not, see");
		notice("<http://www.gnu.org/licenses/>.");
		notice("");
		notice("\002Your IRC client application needs to send these three");
		notice("\002commands in order to connect successfully:");
		notice("");
		notice("    PASS <pw>");
		notice("    NICK <nick>[[:<username>]:<password>]@<network>");
		notice("    USER * * * :<real>");
		notice("");
		notice("<pw> is set in chatproxy3.conf (\"password\" directive);");
		notice("<nick>, <username>, <password> are <network> specific;");
		notice("and supported <network>s are: \002onet\017, \002wp\017, \002interia\017.");
		notice("");
		notice("\002In Irssi, all this can be done simply using:");
		notice("");
		notice("    /set real_name <real>");
		notice("    /connect " + local_host_ + " " + local_port_ + " <pw>");
		notice("       <nick>[[:<username>]:<password>]@<network>");
		notice("");
		notice("\002\0034ERROR\017\002: \017" + std::string(e.what()));
		notice("");
		tcp_.end();
		return;
	}

	notice("");
	notice("Hi, \002" + chat_nick_ + "\017, how are you?");
	notice("Get ready for a ride!");
	notice("");

	auth_ok_ = true;
	tcp_.timeout(360);
	chat_->init();
}

void Session::adm (const std::string& cmd)
{
	privmsg("nutitn heer yet; u sais:");
	privmsg("  \"" + cmd + "\"");
}
