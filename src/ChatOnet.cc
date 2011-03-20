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

#include "ChatOnet.h"
#include "Session.h"
#include "Config.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>

ChatOnet::ChatOnet (Session& session)
	: Chat(session),
	  tcp_(session.io_service_,
			boost::bind(&ChatOnet::handle_read, this),
			boost::bind(&ChatOnet::handle_end, this, _1),
			boost::bind(&ChatOnet::handle_connect, this)),
	  sent_bl_notice_(4),
	  re_mod_c_("%C([0-9a-f]*)%"), re_mod_f_("%F[0-9a-z:]+%"), re_mod_i_("%I([0-9A-Za-z_]+)%"),
	  re_mod_io_("(\\s|^)//([0-9A-Za-z_]+)(\\s|$)"),
	  re_mod_ci_("\003(\\d?\\d?)"),
	  channel_("\x94\xd4\xd9\xd2\xe5\xe1\xe3\xe0\xe9\xea"),
	  rm_mod_c_(session.cnf_.get<std::string>("onet_colors") == "no"),
	  tr_mod_c_(!rm_mod_c_ && session.cnf_.get<std::string>("onet_colors") == "mirc"),
	  rm_mod_f_(session.cnf_.get<std::string>("onet_fonts") == "no"),
	  tr_mod_i_(session.cnf_.get<std::string>("onet_emots") != "no")
{
	Session::brot(channel_, -113);

	typedef boost::bimap<std::string, unsigned int>::value_type v;
	colors_.insert(v("959595", 14));
	colors_.insert(v("990033", 5));
	colors_.insert(v("c86c00", 7));
	colors_.insert(v("623c00", 5));
	colors_.insert(v("ce00ff", 13));
	colors_.insert(v("e40f0f", 4));
	colors_.insert(v("3030ce", 12));
	colors_.insert(v("008100", 3));
	colors_.insert(v("1a866e", 10));
	colors_.insert(v("006699", 11));
	colors_.insert(v("8800ab", 6));
	colors_.insert(v("0f2ab1", 2));
	colors_.insert(v("ff6500", 7));
	colors_.insert(v("ff0000", 4));
	colors_.insert(v("000000", 0));
	colors_.insert(v("", 0));
}

void ChatOnet::init()
{
	notice("");
	notice("Getting UO key...");

	http_.async_get("http://secure.onet.pl/_s/kropka/1?DV=secure",
		boost::bind(&ChatOnet::http_handler, this, 0, _1, _2, shared_from_this()));
}

void ChatOnet::process(std::vector<std::string>& im)
{
	/* repair "synced to #[chan] in oo sec" =) */
	if (im.size() == 2 && im[0] == "MODE" && (im[1][0] == '#' || im[1][0] == '+')) {
		session_.tcp_.write(":chatproxy3 324 " + session_.chat_nick_ + " " + im[1] + " +\r\n");
		return;
	}
	else if (im.size() == 2 && im[0] == "WHO" && (im[1][0] == '#' || im[1][0] == '+')) {
		session_.tcp_.write(":chatproxy3 315 " + session_.chat_nick_ + " " + im[1] + " :End of WHO list.\r\n");
		return;
	}
	else if (im.size() == 3 && im[0] == "MODE" && (im[1][0] == '#' || im[1][0] == '+') && im[2] == "b") {
		session_.tcp_.write(":chatproxy3 368 " + session_.chat_nick_ + " " + im[1] + " :End of Channel Ban List\r\n");
		return;
	}

	std::vector<std::string> afterwards;

	/* privs */
	if (im.size() >= 2 && im[1][0] == '+')
		im[1][0] = '^';

	if (im[0] == "MODE" || im[0] == "WHO" || im[0] == "NICK" || im[0] == "PROTOCTL") {
		if (sent_bl_notice_) {
			notice("");
			notice("MODE/WHO/NICK/PROTOCTL commands have been disabled.");
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

	if (im[0] == "LIST")
		im[0] = "SLIST";
	else if (im.size() >= 3 && im[0] == "MODERNOTICE")
		session_.tcp_.write(":" + session_.chat_nick_ + "!fake@chatproxy3 NOTICE " + im[1] + " :" + im[2] + "\r\n");
	else if (im.size() >= 5 && im[0] == "MODERMSG")
		session_.tcp_.write(":\00314" + session_.chat_nick_ + "\003\002:\002" + im[1] + "!fake@chatproxy3 PRIVMSG "
			+ im[3] + " :" + im[4] + "\r\n");
	else if (im[0] == "QUIT") {
		if (im.size() > 1)
			im[1] += Session::signature_app_;
		else
			im.push_back(Session::signature_onl_);
	}
	else if (im[0] == "PART") {
		if (im[1].find(channel_) != std::string::npos)
			/* make "/part $channel_" act as "/cycle" */
			afterwards.push_back("JOIN " + channel_ + "\r\n");

		if (im.size() > 2)
			im[2] += Session::signature_app_;
		else
			im.push_back(Session::signature_onl_);
	}
	else if (im[0] == "INVITE" && im.size() >= 3 && im[2][0] == '+')
		im[2][0] = '^';

	if (tr_mod_i_) {
		/* //foobar -> %Ifoobar% */
		size_t last = im.size() - 1;
		im[last] = boost::regex_replace(im[last], re_mod_io_, "$1%I$2%$3");
	}

	if (tr_mod_c_) {
		/* mirc colors -> onet */
		size_t last = im.size() - 1;
		boost::smatch match;
		while (boost::regex_search(im[last], match, re_mod_ci_)) {
			std::string col = "";
			try {
				col = colors_.right.at(boost::lexical_cast<unsigned int>(match[1]));
			}
			catch (...) { }
			boost::replace_all(im[last], std::string(match[0]), "%C" + col + "%");
		}
	}

	tcp_.write(Session::conv(charset_, "ISO8859-2", Session::build_im(im)));
	for (size_t i = 0; i < afterwards.size(); i++)
		tcp_.write(Session::conv(charset_, "ISO8859-2", afterwards[i]));
}

void ChatOnet::connect ()
{
	tcp_.timeout(10);
	tcp_.vhost(session_.cnf_.get<std::string>("vhost"));
	tcp_.connect("czat-app.onet.pl", 5015);
}

void ChatOnet::handle_read ()
{
	bool do_send = true;
	std::vector<std::string> im(Session::parse_im(Session::conv("ISO8859-2", charset_, tcp_.data())));

	if (im.size() >= 3)
		boost::to_upper(im[1]);

	{
		/* priv chans prefix */
		size_t n = im.size();
		if (n && !(n >= 2 && im.size() >= 2 && (im[1] == "INVITE" || im[1] == "PART" || im[1] == "341"))) n--;
		for (size_t i = 2; i < n; i++) if (im[i][0] == '^') im[i][0] = '+';
	}

	if (im.size() >= 4 && im[1] == "801") {
		notice("");
		notice("AUTHKEY     = \"" + im[3] + "\"");
		std::string auth(authkey(im[3]));
		notice("transformed = \"" + auth + "\"");
		notice("");

		tcp_.write(Session::conv(charset_, "ISO8859-2",
			"NICK " + session_.chat_nick_ + "\r\n"
			+ "AUTHKEY " + auth + "\r\n"
			+ "USER * " + uokey_ + " czat-app.onet.pl :" + session_.chat_real_
			+ Session::signature_app_ + "\r\n"));
		do_send = false;
	}
	else if (im.size() >= 5 && im[1] == "433") {
		/* nick already taken */
		notice("");
		notice("\0034AUTH error: \"\017" + im[4] + "\017\0034\".");
		notice("");
		session_.end();
		return;
	}
	else if (im.size() >= 4 && im[1] == "005") {
		for (size_t i = 3; i < im.size(); i++)
			if (im[i] == "CHANTYPES=^#") {
				im[i] = "CHANTYPES=#+";
				break;
			}
	}
	else if (im.size() >= 4 && im[1] == "MODE" && (im[3] == "-W" || im[3] == "+W" || im[3] == "+b" || im[3] == "-b"))
		do_send = false;
	else if (im.size() >= 8 && im[1] == "817") {
		std::vector<std::string> nim;
		nim.push_back(":history");
		nim.push_back("NOTICE");
		nim.push_back(im[3]);
		try {
			nim.push_back(Session::epoch_to_human(boost::lexical_cast<unsigned int>(im[4]), 1) + " <" + im[5] + "> " + im[7]);
		}
		catch (...) {
			nim.push_back(im[4] + " <" + im[5] + "> " + im[7]);
		}
		im = nim;
	}
	else if (im.size() >= 4 && im[1] == "MODERNOTICE")
		im[1] = "NOTICE";
	else if (im.size() >= 6 && im[1] == "MODERMSG") {
		im[0].insert(1, "\00314");
		im[0].insert(im[0].find('!'), "\003\002:\002" + im[2]);
		im[1] = "PRIVMSG";
		im.erase(im.begin() + 2, im.begin() + 4);
	}
	else if (im.size() >= 2 && im[1] == "376") {
		notice("");
		notice("Useful <channel> maintenance commands:");
		notice("");
		notice("  /quote busy 1/0");
		notice("  /quote ns info <nick>");
		notice("  /quote ns set <property> <value>");
		notice("  /quote cs info <channel>");
		notice("  /quote rs info");
		notice("  /quote cs homes");
		notice("  /quote cs register <channel>");
		notice("  /quote cs drop <channel>");
		notice("  /quote cs transfer <channel> <nick>");
		notice("  /quote cs op <channel> add/del <nick>");
		notice("  /quote cs halfop <channel> add/del <nick>");
		notice("  /quote cs voice <channel> add/del <nick>");
		notice("  /quote cs ban <channel> add/del <nick>");
		notice("  /quote cs banip <channel> add/del <nick>");
		notice("  /quote cs set <channel> private on/off");
		notice("  /quote cs set <channel> moderated on/off");
		notice("  /quote cs set <channel> topic <topic>");
		notice("");
		notice("A good idea would be to set /PRIV alias -- to invite");
		notice("others to privs. In Irssi, you can just type:");
		notice("");
		notice("  /alias PRIV QUOTE PRIV $-");
		notice("  /priv <nick>");
		notice("");

		tcp_.write("JOIN " + channel_ + "\r\n");
	}
	else if (im.size() >= 2 && im[1] == "818") {
		chan_list_.clear();
		do_send = 0;
	}
	else if (im.size() >= 4 && im[1] == "819") {
		std::vector<std::string> x;
		Channel tmp;
		boost::split(x, im[3], boost::is_any_of(":,"), boost::token_compress_off);
		for (size_t i = 0; i < x.size(); i += 3) {
			tmp.name = x[i];
			tmp.topic = x[i + 1];
			try {
				tmp.num = boost::lexical_cast<size_t>(x[i + 2]);
			}
			catch (...) {
				tmp.num = 0;
			}
			chan_list_.push_back(tmp);
		}
		do_send = 0;
	}
	else if (im.size() >= 2 && im[1] == "820") {
		std::sort(chan_list_.begin(), chan_list_.end());
		for (size_t i = 0; i < chan_list_.size(); i++)
			session_.tcp_.write(":chatproxy3 322 " + session_.chat_nick_ + " " + chan_list_[i].name + " "
				+ boost::lexical_cast<std::string>(chan_list_[i].num) + " :"
				+ chan_list_[i].topic + "\r\n");
		session_.tcp_.write(":chatproxy3 323 " + session_.chat_nick_ + " :End of LIST\r\n");
		do_send = 0;
	}
	else if (im.size() >= 4 && im[1] == "NOTICE" && im[0].length() >= 13 && im[0].substr(im[0].length() - 13) == "@service.onet") {
		boost::regex re_epoch("[1-9][0-9]{7}[0-9]+");
		boost::smatch match;
		while (boost::regex_search(im[im.size() - 1], match, re_epoch)) {
			try {
				boost::replace_all(im[im.size() - 1], std::string(match[0]), "\"" + Session::epoch_to_human(boost::lexical_cast<unsigned int>(match[0])) + "\"");
			}
			catch (...) {
				break;
			}
		}
	}
	else if (im.size() >= 4 && im[1] == "INVITE" && im[3] == channel_)
		tcp_.write("JOIN " + channel_ + "\r\n");

	if (do_send) {
		/* colors, font, images */
		size_t last = im.size() - 1;
		if (rm_mod_c_)
			im[last] = boost::regex_replace(im[last], re_mod_c_, "");
		else if (tr_mod_c_) {
			boost::smatch match;
			while (boost::regex_search(im[last], match, re_mod_c_)) {
				unsigned int col = 0;
				std::string cid(match[1]);
				boost::to_lower(cid);
				try {
					col = colors_.left.at(cid);
				}
				catch (...) { }
				boost::replace_all(im[im.size() - 1], std::string(match[0]), std::string("\003") + (col ? boost::lexical_cast<std::string>(col) : ""));
			}
		}
		if (rm_mod_f_)
			im[last] = boost::regex_replace(im[last], re_mod_f_, "");
		if (tr_mod_i_)
			im[last] = boost::regex_replace(im[last], re_mod_i_, "<$1>");

		/* %% */
		if (!rm_mod_f_ || !tr_mod_i_ || (!rm_mod_c_ && !tr_mod_c_)) {
			size_t p = std::string::npos;
			while ((p = im[last].find("%%", ++p)) != std::string::npos)
				im[last].replace(p, 2, "%");
		}

		/* display */
		session_.tcp_.write(Session::build_im(im));
	}

	tcp_.read_until("\n");
}

void ChatOnet::handle_end (const std::string& reason)
{
	ending_ = true;
	session_.end("ChatOnet: " + reason);
}

void ChatOnet::handle_connect ()
{
	tcp_.write("AUTHKEY\r\n");
	tcp_.timeout(360);
	tcp_.read_until("\n");
}

void ChatOnet::http_handler (int stage, bool error, boost::shared_ptr<std::string> data, boost::shared_ptr<ChatOnet> me)
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
		notice("  kropka/1...");
		if (session_.chat_password_.empty()) {
			std::string nick(Session::conv(charset_, "ISO8859-2", session_.chat_nick_));
			http_.async_post("http://czat.onet.pl/include/ajaxapi.xml.php3",
				"api_function=getUoKey&params=a:3:{s:4:\"nick\";s:"
					+ boost::lexical_cast<std::string>(nick.length())
					+ ":\"" + nick + "\";s:8:\"tempNick\";i:1;s:7:\"version\";s:3:\"ksz\";}",
				boost::bind(&ChatOnet::http_handler, this, 3, _1, _2, me));
		}
		else {
			http_.async_get("http://czat.onet.pl/client.html?ch=chatproxy3&e=0",
				boost::bind(&ChatOnet::http_handler, this, 1, _1, _2, me));
		}
	}
	else if (stage == 1) {
		notice("  client.html...");
		std::string nick(Session::conv(charset_, "ISO8859-2", session_.chat_nick_));
		std::string pass(Session::conv(charset_, "ISO8859-2", session_.chat_password_));
		http_.async_post("https://secure.onet.pl/mlogin.html",
			"r=&url=&login=" + nick + "&haslo=" + pass + "&app_id=20&ssl=1&ok=1",
			boost::bind(&ChatOnet::http_handler, this, 2, _1, _2, me));
	}
	else if (stage == 2) {
		notice("  mlogin.html...");
		std::string nick(Session::conv(charset_, "ISO8859-2", session_.chat_nick_));
		http_.async_post("http://czat.onet.pl/include/ajaxapi.xml.php3",
			"api_function=getUoKey&params=a:3:{s:4:\"nick\";s:"
				+ boost::lexical_cast<std::string>(nick.length())
				+ ":\"" + nick + "\";s:8:\"tempNick\";i:0;s:7:\"version\";s:3:\"ksz\";}",
			boost::bind(&ChatOnet::http_handler, this, 3, _1, _2, me));
	}
	else if (stage == 3) {
		notice("  ajaxapi.xml...");
		boost::regex uo("<uoKey>([^<]+)"), err("err_text=\"([^\"]*)"), nick("<zuoUsername>([^<]+)");
		boost::smatch m;
		if (!boost::regex_search(*data, m, uo)) {
			notice("");
			if (boost::regex_search(*data, m, err))
				notice("\0034AUTH error: \"\017" + Session::conv("ISO8859-2", charset_, m[1]) + "\017\0034\".");
			else
				notice("\0034AUTH error: \017\002?");
			notice("");
			session_.end();
		}
		else {
			uokey_ = m[1];
			notice("  uokey = \"" + uokey_ + "\"");
			notice("");
			if (boost::regex_search(*data, m, nick))
				session_.chat_nick_ = Session::conv("ISO8859-2", charset_, m[1]);
			notice("Connecting...");
			notice("");
			connect();
		}
	}
}

std::string ChatOnet::authkey (const std::string& key)
{
	if (key.size() != 16)
		return "err";

	int f1[] = {
		29, 43, 7, 5, 52, 58, 30, 59, 26, 35,
		35, 49, 45, 4, 22, 4, 0, 7, 4, 30,
		51, 39, 16, 6, 32, 13, 40, 44, 14, 58,
		27, 41, 52, 33, 9, 30, 30, 52, 16, 45,
		43, 18, 27, 52, 40, 52, 10, 8, 10, 14,
		10, 38, 27, 54, 48, 58, 17, 34, 6, 29,
		53, 39, 31, 35, 60, 44, 26, 34, 33, 31,
		10, 36, 51, 44, 39, 53, 5, 56 };

	int f2[] = {
		7, 32, 25, 39, 22, 26, 32, 27, 17, 50,
		22, 19, 36, 22, 40, 11, 41, 10, 10, 2,
		10, 8, 44, 40, 51, 7, 8, 39, 34, 52,
		52, 4, 56, 61, 59, 26, 22, 15, 17, 9,
		47, 38, 45, 10, 0, 12, 9, 20, 51, 59,
		32, 58, 19, 28, 11, 40, 8, 28, 6, 0,
		13, 47, 34, 60, 4, 56, 21, 60, 59, 16,
		38, 52, 61, 44, 8, 35, 4, 11 };

	int f3[] = {
		60, 30, 12, 34, 33, 7, 15, 29, 16, 20,
		46, 25, 8, 31, 4, 48, 6, 44, 57, 16,
		12, 58, 48, 59, 21, 32, 2, 18, 51, 8,
		50, 29, 58, 6, 24, 34, 11, 23, 57, 43,
		59, 50, 10, 56, 27, 32, 12, 59, 16, 4,
		40, 39, 26, 10, 49, 56, 51, 60, 21, 37,
		12, 56, 39, 15, 53, 11, 33, 43, 52, 37,
		30, 25, 19, 55, 7, 34, 48, 36 };

	int p1[] = {
		11, 9, 12, 0, 1, 4, 10, 13, 3, 6,
		7, 8, 15, 5, 2, 14 };

	int p2[] = {
		1, 13, 5, 8, 7, 10, 0, 15, 12, 3,
		14, 11, 2, 9, 6, 4 };

	int ai[16], ai1[16], i, j, k, l, i1, j1, k1;

	for (i = 0; i < 16; i++) {
		char c = key[i];
		ai[i] = (c > '9' ? c > 'Z' ? (c - 97) + 36 : (c - 65) + 10 : c - 48);
	}

	for (j = 0; j < 16; j++)
		ai[j] = f1[ai[j] + j];

	for (i = 0; i < 16; i++)
		ai1[i] = ai[i];

	for (k = 0; k < 16; k++)
		ai[k] = (ai[k] + ai1[p1[k]]) % 62;

	for (l = 0; l < 16; l++)
		ai[l] = f2[ai[l] + l];

	for (i = 0; i < 16; i++)
		ai1[i] = ai[i];

	for (i1 = 0; i1 < 16; i1++)
		ai[i1] = (ai[i1] + ai1[p2[i1]]) % 62;

	for (j1 = 0; j1 < 16; j1++)
		ai[j1] = f3[ai[j1] + j1];

	std::string nkey;

	for (k1 = 0; k1 < 16; k1++) {
		char l1 = ai[k1];
		ai[k1] = l1 >= 10 ? l1 >= 36 ? (97 + l1) - 36 : (65 + l1) - 10 : 48 + l1;
		nkey.push_back((char )ai[k1]);
	}

	return nkey;
}
