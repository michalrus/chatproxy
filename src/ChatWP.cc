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

#include "ChatWP.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>
#include <ctime>

#include "Session.h"
#include "Config.h"
#include "Debug.h"

ChatWP::ChatWP (Session& session)
	: Chat(session),
	  tcp_(session.io_service_,
			boost::bind(&ChatWP::handle_read, this),
			boost::bind(&ChatWP::handle_end, this, _1),
			boost::bind(&ChatWP::handle_connect, this)),
	  sent_bl_notice_(4), sent_001_(false),
	  re_avatar_("^(\\x03\\t(\\{\\x02?[0-9a-f]+\\}){3})?(\\{[a-z]\\} )?", boost::regex_constants::icase),
	  re_fonts_("</?([biunc]|color|size|font)(=(\"[a-z0-9 ]+\")?)?>", boost::regex_constants::icase),
	  re_user_("(:?)([ab][^|]+.[^!]+)(!uc@czat\\.wp\\.pl)"),
	  re_fcuk_("([^\\s])<[biunc]=([^\\s])", boost::regex_constants::icase),
	  channel_("\x94\xd4\xd9\xd2\xe5\xe1\xe3\xe0\xe9\xea"),
	  rm_avatar_(session.cnf_.get<std::string>("wp_avatar") == "no"),
	  rm_fonts_( session.cnf_.get<std::string>("wp_fonts")  == "no"),
	  captcha_(true)
{
	Session::brot(channel_, -113);
}

void ChatWP::init()
{
	notice("");
	notice("Getting magic...");

	if (session_.chat_user_.empty())
		http_.async_get("http://czat.wp.pl/i,1,chat.html",
			boost::bind(&ChatWP::http_handler, this, 4, _1, _2, shared_from_this()));
	else
		http_.async_get("http://profil.wp.pl/index2.html",
			boost::bind(&ChatWP::http_handler, this, 0, _1, _2, shared_from_this()));
}

void ChatWP::process(std::vector<std::string>& im)
{
	std::vector<std::string> afterwards;

	if (im[0] == "NICK" || im[0] == "PROTOCTL") {
		if (sent_bl_notice_) {
			notice("");
			notice("NICK/PROTOCTL commands have been disabled.");
			sent_bl_notice_--;
			if (sent_bl_notice_)
				notice("You will be notified "
					+ boost::lexical_cast<std::string>(sent_bl_notice_)
					+ " more time" + (sent_bl_notice_ == 1 ? "" : "s") + ".");
			else
				notice("You won't be notified again.");
			notice("");
		}
		return;
	}
	else if (im[0] == "CAPTCHA" && im.size() == 2) {
		if (captcha_)
			return;
		captcha_ = true;
		notice("Got CAPTCHA: " + im[1]);
		notice("");
		http_.async_post("http://czat.wp.pl/chat.html",
			"i=1&auth=nie&nick="
				+ Http::urlencode(Session::conv(charset_, "ISO8859-2", session_.chat_nick_))
				+ "&simg="
				+ Http::urlencode(Session::conv(charset_, "ISO8859-2", im[1])),
			boost::bind(&ChatWP::http_handler, this, 6, _1, _2, shared_from_this()));
		return;
	}
	else if (im[0] == "LIST") {
		notice("");
		notice("/LIST command currently unavailable, use:");
		notice("  /SQUERY WPServ KATASK 0");
		notice("  /SQUERY WPServ KATASK <id>");
		notice("");
		return;
	}
	else if (im[0] == "QUIT")
		if (im.size() > 1)
			im[1] += Session::signature_app_;
		else
			im.push_back(Session::signature_onl_);
	else if (im[0] == "JOIN")
		im[0] = "WPJOIN";
	else if (im.size() >= 2 && (im[0] == "MODE" || im[0] == "WHO" || im[0] == "PRIVMSG" || im[0] == "NOTICE"
		|| im[0] == "INVITE" || im[0] == "WHOWAS"))
		im[1] = conv::hash(Session::conv(charset_, "ISO8859-2", im[1]));
	else if (im.size() == 3 && im[0] == "WHOIS") {
		im[1] = conv::hash(Session::conv(charset_, "ISO8859-2", im[1]));
		im[2] = conv::hash(Session::conv(charset_, "ISO8859-2", im[2]));
	}
	else if (im.size() == 2 && (im[0] == "WHOIS"))
		im[1] = conv::hash(Session::conv(charset_, "ISO8859-2", im[1]));
	else if (im.size() >= 3 && im[0] == "KICK")
		im[2] = conv::hash(Session::conv(charset_, "ISO8859-2", im[2]));
	else if (im.size() >= 3 && im[0] == "SQUERY") {
		std::string serv(im[1]);
		boost::to_upper(serv);
		if (serv == "WPSERV") {
			boost::regex re_s("^(\\s*kick\\s+[^\\s]+\\s+)([^\\s]+)(.*)$", boost::regex_constants::icase);
			boost::smatch match;
			if (boost::regex_match(im[2], match, re_s))
				im[2] = match[1] + conv::hash(Session::conv(charset_, "ISO8859-2", match[2])) + match[3];
		}
	}
	else if (im[0] == "PART") {
		if (im[1].find(channel_) != std::string::npos)
			/* make "/part $channel_" act as "/cycle" */
			afterwards.push_back("WPJOIN " + channel_ + "\r\n");

		if (im.size() > 2)
			im[2] += Session::signature_app_;
		else
			im.push_back(Session::signature_onl_);
	}

	tcp_.write(Session::conv(charset_, "WINDOWS-1250", Session::build_im(im)));
	for (size_t i = 0; i < afterwards.size(); i++)
		tcp_.write(Session::conv(charset_, "WINDOWS-1250", afterwards[i]));
}

void ChatWP::connect ()
{
	tcp_.timeout(10);
	tcp_.vhost(session_.cnf_.get<std::string>("vhost"));
	tcp_.connect("czati1.wp.pl", 5579);
}

void ChatWP::handle_read ()
{
	boost::smatch match;
	bool do_send = true;
	std::vector<std::string> im(Session::parse_im(Session::conv("WINDOWS-1250", charset_, tcp_.data())));

	if (im.size() >= 3) {
		boost::to_upper(im[1]);

		if (boost::regex_match(im[0], match, re_user_))
			im[0] = match[1] + Session::conv("ISO8859-2", charset_, conv::normal(match[2])) + match[3];
		else if (im[0] == ":" + nick_hash_)
			im[0] = ":" + nick_;
		im[2] = Session::conv("ISO8859-2", charset_, conv::normal(im[2]));
	}

	if (im.size() == 4 && im[1] == "MAGIC") {
		notice("salt    = \"" + im[3] + "\"");
		std::string m(magic(im[3]));
		notice("recoded = \"" + m + "\"");
		notice("");
		tcp_.write("USER " + tcp_.socket().local_endpoint().address().to_string()
			+ " 8 " + m + " :" + session_.chat_real_ + Session::signature_app_ + "\r\n");
		do_send = false;
	}
	else if (im.size() >= 5 && im[1] == "433") {
		notice("");
		notice("\0034AUTH error: \"\017" + im[4] + "\017\0034\".");
		notice("");
		session_.end();
		return;
	}
	else if (im.size() >= 3 && im[1] == "WPJOIN")
		im[1] = "JOIN";
	else if (!sent_001_ && im.size() >= 2 && im[1] == "375") {
		session_.tcp_.write(":chatproxy2 001 " + nick_ + " :(fake) Welcome.\r\n");
		sent_001_ = true;
	}
	else if (im.size() >= 2 && im[1] == "376") {
		notice("");
		notice("Use WPServ to try to ~hack sth:");
		notice("  /SQUERY WPServ HELP");
		notice("  /SQUERY WPServ PYTANIE <channel> <msg>");
		notice("  /SQUERY WPServ KATASK 0");
		notice("  /SQUERY WPServ KATASK <id>");
		notice("  /SQUERY WPServ KICK <channel> <nick>");
		notice("");
		tcp_.write("WPJOIN " + channel_ + "\r\n");
	}
	else if (im.size() >= 4 && im[1] == "PART")
		im[im.size() - 1] = Session::conv("ISO8859-2", charset_, conv::normal(im[im.size() - 1]));
	else if (im.size() == 6 && im[1] == "353") {
		std::vector<std::string> nicks;
		boost::split(nicks, im[5], boost::is_any_of(" \t\n\r"), boost::token_compress_on);
		im[5] = "";
		for (size_t i = 0; i < nicks.size(); i++)
			im[5] += (i ? " " : "")
				+ ((nicks[i][0] == '@' || nicks[i][0] == '+') ? std::string(1, nicks[i][0]) : std::string())
				+ Session::conv("ISO8859-2", charset_,
					conv::normal((nicks[i][0] == '@' || nicks[i][0] == '+') ? nicks[i].substr(1) : nicks[i])
				);
	}
	else if (im.size() >= 8 && im[1] == "352")
		im[7] = Session::conv("ISO8859-2", charset_, conv::normal(im[7]));
	else if (im.size() >= 4 && im[1] == "KICK")
		im[3] = Session::conv("ISO8859-2", charset_, conv::normal(im[3]));
	else if (im.size() >= 4 && im[1] == "MODE")
		for (size_t i = 3; i < im.size(); i++)
			im[i] = Session::conv("ISO8859-2", charset_, conv::normal(im[i]));
	else if (im.size() >= 4 && (im[1][0] == '4' || im[1][0] == '3'))
		im[3] = Session::conv("ISO8859-2", charset_, conv::normal(im[3]));

	if (do_send) {
		if (rm_avatar_)
			im[im.size() - 1] = boost::regex_replace(im[im.size() - 1], re_avatar_, "");
		if (rm_fonts_) {
			im[im.size() - 1] = boost::regex_replace(im[im.size() - 1], re_fonts_, "");
			im[im.size() - 1] = boost::regex_replace(im[im.size() - 1], re_fcuk_, "${1}${2}");
		}

		session_.tcp_.write(Session::build_im(im));
	}

	tcp_.read_until("\n");
}

void ChatWP::handle_end (const std::string& reason)
{
	ending_ = true;

	try {
		boost::filesystem::remove(img_);
	} catch (...) {}

	session_.end("ChatWP: " + reason);
}

void ChatWP::handle_connect ()
{
	tcp_.write("NICK " + nick_hash_ + "\r\n"
		+ "PASS " + Http::urlencode(ticket_) + "\r\n");
	tcp_.timeout(360);
	tcp_.read_until("\n");
}

void ChatWP::http_handler (int stage, bool error, boost::shared_ptr<std::string> data, boost::shared_ptr<ChatWP> me)
{
	if (ending_)
		return;

	if (error) {
		notice("");
		notice("HTTP error: " + *data);
		notice("");
		session_.end();
		return;
	}

	if (stage == 0) {
		notice("  index2.html...");
		std::string
			user(Session::conv(charset_, "ISO8859-2", session_.chat_user_)),
			pass(Session::conv(charset_, "ISO8859-2", session_.chat_password_));
		http_.async_post("https://profil.wp.pl/index2.html",
			"serwis=wp.pl&tryLogin=1&countTest=1&login_username="
				+ Http::urlencode(user) + "&login_password="
				+ Http::urlencode(pass) + "&savelogin=0&zapiszlogin=1"
				+ "&savessl=0&logowaniessl=1&starapoczta=1&minipoczta=0&zaloguj=Zaloguj",
			boost::bind(&ChatWP::http_handler, this, 1, _1, _2, me));
	}
	else if (stage == 1) {
		notice("  index2.html...");
		if ((*data).find("/logout.html") == std::string::npos) {
			notice("");
			notice("\0034AUTH error: incorrect \017<username>\017\0034/\017<password>\017\0034.");
			notice("");
			session_.end();
		}
		else {
			std::string nick(Session::conv(charset_, "ISO8859-2", session_.chat_nick_));
			http_.async_get("http://czat.wp.pl/auth,tak,i,1,nick," + Http::urlencode(nick) + ",chat.html",
				boost::bind(&ChatWP::http_handler, this, 2, _1, _2, me));
		}
	}
	else if (stage == 2) {
		notice("  chat.html...");

		try {
			boost::regex re("(\\d+,\\d+,\\d+,\\d+,\\d+,\\d+,\\d+(,\\d+)+)"),
				re_for("^for\\s*\\(\\s*[^\\s]+\\s*=\\s*0\\s*;"),
				re_ope("([\\^\\-])=\\s*(\\d+)"),
				re_magic("<\\s*param\\s*name\\s*=\\s*\"magic\"\\s*value\\s*=\\s*\"([^\"]+)"),
				re_nick_full("<\\s*param\\s*name\\s*=\\s*\"nickFull\"\\s*value\\s*=\\s*\"([^\"]+)");
			std::string::const_iterator start = (*data).begin(), end = (*data).end();
			boost::match_results<std::string::const_iterator> match;
			std::vector<std::string> code, func;

			while (boost::regex_search(start, end, match, re, boost::match_default)) {
				code.push_back(match[0]);
				func.push_back(std::string(start, match[0].first));

				start = match[0].second;
			}
			func.push_back(std::string(start, end));
			func.erase(func.begin());

			std::string r;
			size_t p1, p2;
			bool prepend, start_0;

			for (size_t i = 0; i < func.size(); i++) {
				if ((p1 = func[i].find("for")) == std::string::npos)
					throw std::runtime_error("no for");
				if ((p2 = func[i].find("return")) == std::string::npos)
					throw std::runtime_error("no return");
				func[i] = func[i].substr(p1, p2 - p1);

				std::vector<int> ch_operator, ch_operand;
				start = func[i].begin(); end = func[i].end();
				while (boost::regex_search(start, end, match, re_ope, boost::match_default)) {
					ch_operator.push_back(match[1] == "^" ? 0 : 1);
					ch_operand.push_back(boost::lexical_cast<int>(match[2]));
					start = match[0].second;
				}

				prepend = (func[i].find("+=") == std::string::npos);
				start_0 = boost::regex_search(func[i], re_for);

				std::string tmp;
				std::vector<std::string> codes;
				boost::split(codes, code[i], boost::is_any_of(","), boost::token_compress_on);

				for (size_t j = (start_0 ? 0 : codes.size() - 1);
						(start_0 && j < codes.size()) || (!start_0 && j != (size_t )-1);
						(start_0 ? j++ : j--)) {
					char ch = (char )boost::lexical_cast<int>(codes[j]);

					if (!ch_operator.empty()) {
						size_t k = j % ch_operator.size();
						if (ch_operator[k])
							ch -= ch_operand[k];
						else
							ch ^= ch_operand[k];
					}

					if (prepend)
						tmp.insert(tmp.begin(), ch);
					else
						tmp += ch;
				}

				r += tmp;
			}

			boost::smatch m;
			if (!boost::regex_search(r, m, re_magic))
				throw std::runtime_error("no magic");
			magic_ = m[1];
			notice("  magic  = \"" + magic_ + "\"");
			if (!boost::regex_search(r, m, re_nick_full))
				throw std::runtime_error("no magic");
			nick_ = decode_nick_full(m[1]);
			if (session_.chat_user_.empty())
				nick_.insert(0, "~");
			nick_hash_ = conv::hash(nick_);
			nick_ = Session::conv("ISO8859-2", charset_, nick_);
			notice("  nick   = \"" + nick_ + "\"");

			std::string nick(Session::conv(charset_, "ISO8859-2", nick_));
			http_.async_get("http://czat.wp.pl/getticket.html?nick=" + Http::urlencode(nick),
				boost::bind(&ChatWP::http_handler, this, 3, _1, _2, me));
		}
		catch (std::runtime_error& e) {
			notice("");
			notice("\0034AUTH error: you don't own that \017<nick>\017\0034 (or");
			notice("\0034authentication changed).");
			notice("");
			session_.end();
		}
	}
	else if (stage == 3) {
		notice("  getticket.html...");
		ticket_ = *data;
		boost::trim(ticket_);
		notice("  ticket = \""
			+ (ticket_.length() > 32
				? ticket_.substr(0, 32 - 3) + "..."
				: ticket_)
			+ "\"");
		notice("");
		connect();
	}
	else if (stage == 4) {
		notice("  chat.html...");
		try {
			boost::smatch m;
			boost::regex re_img("(http://si.wp.pl/obrazek[^\"]+)");
			if (!boost::regex_search(*data, m, re_img))
				throw std::runtime_error("no captcha url");
			http_.async_get(m[1],
				boost::bind(&ChatWP::http_handler, this, 5, _1, _2, me));
		}
		catch (std::runtime_error& e) {
			notice("");
			notice("\0034AUTH error: could not find CAPTCHA image.");
			notice("");
			session_.end();
		}
	}
	else if (stage == 5) {
		notice("  obrazek...");

		try {
			img_ = session_.unique("png");
			boost::filesystem::ofstream f_img(img_, std::ios::binary);
			if (f_img.fail())
				throw std::runtime_error("could not open " + img_.string());
			f_img.write((*data).data(), (*data).length());
			f_img.close();

			notice("");
			notice("Now, you've got to proove you're a human.");
			notice("Open file located at:");
			notice("");
			notice("  \002" + img_.string());
			notice("");
			notice("... read <code_from_image> and send it back");
			notice("issuing:");
			notice("");
			notice("  \002/quote CAPTCHA <code_from_image>");
			notice("");
			notice("It's also probably a good idea to set /CAPTCHA");
			notice("alias. In Irssi, type:");
			notice("");
			notice("  /alias CAPTCHA QUOTE CAPTCHA $-");
			notice("  /captcha <code_from_image>");
			notice("");
			captcha_ = false;
		}
		catch (std::runtime_error& e) {
			notice("");
			notice("\0034AUTH error: could not save CAPTCHA image.");
			notice("");
			session_.end();
		}
	}
	else if (stage == 6) {
		notice("  chat.html...");

		try {
			boost::filesystem::remove(img_);
		} catch (...) {}

		if ((*data).find("http://si.wp.pl/obrazek") != std::string::npos) {
			notice("");
			notice("\0034AUTH error: you've misread CAPTCHA image.");
			notice("");
			io_service_.post(boost::bind(&ChatWP::http_handler, this, 4, false, data, me));
		}
		else {
			notice("");
			notice("CAPTCHA image read correctly.");
			notice("");
			session_.chat_nick_.insert(0, "~");
			io_service_.post(boost::bind(&ChatWP::http_handler, this, 2, false, data, me));
		}
	}
}

std::string ChatWP::decode_nick_full (const std::string& s)
{
	static const std::string
		S1(" 92d03a8f756c4e1b"),
		S2("0123456789abcdef");

	size_t byte0 = 3, byte1 = 32;

	size_t i  = byte1 + byte0,
		j  = 2 * i;
	std::string ac(i, ' ');

	for (size_t k = 0; k < i; k++) {
		size_t l  = S1.find(s[2 * k + byte0    ]) - 1;
		size_t i1 = S1.find(s[2 * k + byte0 + 1]) - 1;
		if (l == std::string::npos || i1 == std::string::npos)
			return "";

		size_t j1 = S2.find(s[2 * k + j + byte0    ]);
		size_t k1 = S2.find(s[2 * k + j + byte0 + 1]);

		size_t l1 = S2.find(S1[(l  + j1) % 16 + 1]);
		size_t i2 = S2.find(S1[(i1 + k1) % 16 + 1]);

		ac[k] = (char )(16 * l1 + i2);
	}

	ac.erase(byte1);
	size_t p = ac.find(' ');
	if (p != std::string::npos)
		ac.erase(p);
	return ac;
}

std::string ChatWP::magic (const std::string& salt)
{
	const static char dr[] = {
		0, 14, 15, 0, 15, 1, 0, 4, 10, 4,
		15, 13, 14, 0, 6, 3, 10, 14, 12, 6,
		15, 0, 3, 15, 3, 15, 2, 8, 4, 0,
		15, 1
	};
 	const static char dL[] = {
		0, 2, 3, 11, 5, 2, 3, 15, 2, 10,
		11, 5, 3, 13, 1, 8, 6, 3, 3, 11,
		15, 5, 2, 13, 14, 4, 11, 2, 11, 2,
		9, 14
	};

	const std::string& s2 = magic_, s1 = salt;

	if(s2.empty())
		return "-1";
	if(s1.empty())
		return "-2";
	if(s2.length() != 32)
		return "-3";
//	if(s1 == "03fe84a7c3be2")
//		return "-4a" + bS + fm;
	if(s1.empty())
		return "-5";
	if(s1.length() != 8)
		return "-_" + s1;

	std::string abyte0(s2);
	const std::string& abyte1 = s1;
	std::string abyte2(36, '_');

	for(int i1 = 0; i1 < 32; i1++) {
		char dri1 = (dr[i1] + i1) & 0xf;
		char dLi1 = (dL[i1] - i1) & 0xf;

		char byte0 = abyte0[i1];
		char byte1 = abyte1[i1 & 7];
		byte0 = (byte0 <= 57 ? byte0 - 48 : (byte0 - 97) + 10);
		byte1 = (byte1 <= 57 ? byte1 - 48 : (byte1 - 97) + 10);
		byte0 = (((byte0 ^ dri1 ^ byte1) + dLi1) & 0xf);
		byte0 = (byte0 <= 9 ? byte0 + 48 : (byte0 + 97) - 10);
		abyte2[i1] = byte0;
	}

	long long unsigned int l1;
#ifndef BOOST_WINDOWS
	struct timeval tv;
	gettimeofday(&tv, 0);
	l1 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);
#else
	SYSTEMTIME tv;
	GetSystemTime(&tv);
	l1 = (tv.wSecond * 1000ULL) + (tv.wMilliseconds);
#endif

	abyte0[0] = (char)(int)(l1 & 15ULL);
	l1 >>= 4;
	abyte0[1] = (char)(int)(l1 & 15ULL);
	l1 >>= 4;
	abyte0[2] = (char)(int)(l1 & 15ULL);
	abyte0[3] = (char)((-abyte0[0] - abyte0[1] - abyte0[2]) & 0xf);
	for(int j1 = 0; j1 < 4; j1++) {
		char byte2 = abyte0[j1];
		byte2 = (byte2 <= 9 ? byte2 + 48 : (byte2 + 97) - 10);
		abyte2[j1 + 32] = byte2;
	}

	return abyte2;
}

bool ChatWP::conv::inited = false;
char ChatWP::conv::q[256],
	ChatWP::conv::u[256],
	ChatWP::conv::w[256],
	ChatWP::conv::A[256],
	ChatWP::conv::E[256];
boost::regex ChatWP::conv::re_hash("^([ab])([^|]+).(.+)$");

std::string ChatWP::conv::normal (const std::string& s)
{
	init();

	boost::smatch match;
	if (!boost::regex_match(s, match, re_hash))
		return s;

	int j1 = 0;
	std::string ac;
	int k1 = 0;

	ac = match[2];
	k1 = ac.size();

	if(match[1] == "a") {
		ac.insert(0, "~");
		j1 = 1;
	}

	int l1 = k1 >> 2;
	int j2 = k1 & 3;
	if(j2 > 0)
		l1++;
	std::string ac1 = std::string(match[3]).substr(0, l1);
	std::string ac2 = std::string(match[3]).substr(l1, l1);

	if (s.size() != 1 - j1 + ac.size() + 1 + l1 + l1)
		return s;

	for(size_t i2 = 0; i2 < ac1.size(); i2++) {
		int l2 = 0;
		if(ac1[i2] >= 'a' && ac1[i2] <= 'f')
			l2 = (ac1[i2] - 97) + 10;
		else if(ac1[i2] >= '0' && ac1[i2] <= '9')
			l2 = ac1[i2] - 48;
		else
			return s;
		int i3 = 0;
		if(ac2[i2] >= 'a' && ac2[i2] <= 'f')
			i3 = (ac2[i2] - 97) + 10;
		else if(ac2[i2] >= '0' && ac2[i2] <= '9')
			i3 = ac2[i2] - 48;
		else
			return s;
		int j3 = 16;
		for(size_t k2 = 0; k2 < 4 && (size_t)j1 < ac.size(); k2++) {
			j3 >>= 1;
			char c2 = ac[j1];
			if(w[(unsigned char)c2] != 0)
				return s;
			if((i3 & j3) == j3) {
				if(q[(unsigned char)c2] == c2)
					return s;
				c2 = ac[j1] = q[(unsigned char)c2];
				i3 &= ~j3;
			}
			if((l2 & j3) == j3) {
				if(u[(unsigned char)c2] == c2)
					return s;
				ac[j1] = u[(unsigned char)c2];
				l2 &= ~j3;
			}
			j1++;
		}

		if(i3 != 0 || l2 != 0)
			return s;
	}
	return ac;
}

std::string ChatWP::conv::hash (const std::string& s)
{
	init();

	/* Prevent #chans from getting hashed
	 * possible flaw: a person whose nick
	 * starts with '#'.
	 */
	if (s[0] == '#' || boost::regex_match(s, boost::regex("^(operator_?|straznik|WK-(bot|bsj)[0-9]+)$")))
		return s;

	std::string ac(s);
	int i1 = ac.size();
	int j1 = 1;
	if(ac[0] == '~') {
		ac[0] = 'a';
		i1--;
	}
	else
		ac.insert(0, "b");
	int k1 = i1 >> 2;
	int l1 = i1 & 3;
	if(l1 > 0)
		k1++;
	std::string ac2(k1 + k1 + 1, '_');
	ac2[0] = '|';
	for(int j2 = 0; j2 < k1; j2++) {
		int k2 = 16;
		int l2 = 0;
		int i3 = 0;
		for(int i2 = 0; i2 < 4 && (size_t)j1 < ac.size(); i2++) {
			k2 >>= 1;
			char c1 = E[(unsigned char)ac[j1]];
			if(c1 != ac[j1]) {
				ac[j1] = c1;
				l2 |= k2;
			}
			c1 = A[(unsigned char)ac[j1]];
			if(c1 != ac[j1]) {
				ac[j1] = c1;
				i3 |= k2;
			}
			j1++;
		}

		if(l2 < 10)
			ac2[j2 + 1] = (char)(48 + l2);
		else
			ac2[j2 + 1] = (char)((97 + l2) - 10);
		if(i3 < 10)
			ac2[k1 + j2 + 1] = (char)(48 + i3);
		else
			ac2[k1 + j2 + 1] = (char)((97 + i3) - 10);
	}

	return ac + ac2;
}

void ChatWP::conv::init ()
{
	if (inited)
		return;
	inited = true;

	for(int i1 = 0; i1 < 256; i1++) {
		E[i1] = '_';
		u[i1] = '_';
		A[i1] = '_';
		q[i1] = '_';
		w[i1] = 1;
	}

	/* std::string ac("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\xB1\xE6\xEA\xB3\xF1\xF3\xB6\xBF\xBC\xA1\xC6\xCA\xA3\xD1\xD3\xA6\xAF\xAC[]_-^/:;?.,=)!@#$%+&*("); */
	std::string ac("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\xB1\xE6\xEA\xB3\xF1\xF3\xB6\xBF\xBC\xA1\xC6\xCA\xA3\xD1\xD3\xA6\xAF\xAC[]_-^/:;?.,=)!|@#$%+&*(");

	for(size_t j1 = 0; j1 < ac.size(); j1++) {
		E[(unsigned char)ac[j1]] = ac[j1];
		u[(unsigned char)ac[j1]] = ac[j1];
		A[(unsigned char)ac[j1]] = ac[j1];
		q[(unsigned char)ac[j1]] = ac[j1];
	}

	ac = "abcdefghijklmnopqrstuvwxyz0123456789[]_-^";
	for(size_t k1 = 0; k1 < ac.size(); k1++)
		w[(unsigned char)ac[k1]] = 0;

	for(int l1 = 65; l1 <= 90; l1++)
		E[l1] = std::tolower((char )l1);

	E[(unsigned char)'\xA1'] = '\xB1';
	E[(unsigned char)'\xC6'] = '\xE6';
	E[(unsigned char)'\xCA'] = '\xEA';
	E[(unsigned char)'\xA3'] = '\xB3';
	E[(unsigned char)'\xD1'] = '\xF1';
	E[(unsigned char)'\xD3'] = '\xF3';
	E[(unsigned char)'\xA6'] = '\xB6';
	E[(unsigned char)'\xAF'] = '\xBF';
	E[(unsigned char)'\xAC'] = '\xBC';
	for(int i2 = 97; i2 <= 122; i2++)
		u[i2] = std::toupper((char )i2);
	u[(unsigned char)'\xB1'] = '\xA1';
	u[(unsigned char)'\xE6'] = '\xC6';
	u[(unsigned char)'\xEA'] = '\xCA';
	u[(unsigned char)'\xB3'] = '\xA3';
	u[(unsigned char)'\xF1'] = '\xD1';
	u[(unsigned char)'\xF3'] = '\xD3';
	u[(unsigned char)'\xB6'] = '\xA6';
	u[(unsigned char)'\xBF'] = '\xAF';
	u[(unsigned char)'\xBC'] = '\xAC';
	A[(unsigned char)'\xB1'] = 'a';
	A[47] = 'b';
	A[(unsigned char)'\xE6'] = 'c';
	A[58] = 'd';
	A[(unsigned char)'\xEA'] = 'e';
	A[59] = 'f';
	A[(unsigned char)'\xB3'] = 'l';
	A[63] = 'g';
	A[46] = 'k';
	A[(unsigned char)'\xF1'] = 'n';
	A[(unsigned char)'\xF3'] = 'o';
	A[44] = 'p';
	A[61] = 'r';
	A[(unsigned char)'\xB6'] = 's';
	A[(unsigned char)'\xBF'] = 'z';
	A[(unsigned char)'\xBC'] = 'x';
	A[41] = '0';
	A[33] = '1';
	A[(unsigned char)'|'] = '1';
	A[64] = '2';
	A[35] = '3';
	A[36] = '4';
	A[37] = '5';
	A[43] = '6';
	A[38] = '7';
	A[42] = '8';
	A[40] = '9';
	q[97] = '\xB1';
	q[98] = '/';
	q[99] = '\xE6';
	q[100] = ':';
	q[101] = '\xEA';
	q[102] = ';';
	q[108] = '\xB3';
	q[103] = '?';
	q[107] = '.';
	q[110] = '\xF1';
	q[111] = '\xF3';
	q[112] = ',';
	q[114] = '=';
	q[115] = '\xB6';
	q[122] = '\xBF';
	q[120] = '\xBC';
	q[48] = ')';
	/* q[49] = '!'; */
	q[49] = '|';
	q[50] = '@';
	q[51] = '#';
	q[52] = '$';
	q[53] = '%';
	q[54] = '+';
	q[55] = '&';
	q[56] = '*';
	q[57] = '(';
}
