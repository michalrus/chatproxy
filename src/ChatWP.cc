#include "ChatWP.h"

#include <sstream>

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>

#include "EException.h"
#include "Session.h"
#include "Http.h"
#include "Tcp.h"

namespace WPConv {
	void fill ();
	std::string normal (const std::string& s);
	std::string hash (const std::string& s);
	void conv (size_t from, size_t to, std::vector<std::string>& s);
	void conv (size_t from, size_t to, std::string& s);
	char conv (size_t from, size_t to, char ch);
}

ChatWP::ChatWP (Session* session)
	: Chat(session),
	  tokenNeeded_(0),
	  reCut_("(^(\\x03\\t(\\{\\x02?[0-9a-f]+\\}){3})?(\\{[a-z]\\} )?"
		"|</?([biunc]|color|size|font)(=(\"[a-z0-9 ]+\")?)?>)", boost::regex_constants::icase),
	  reUser_("(:?)([ab][^|]+.[^!]+)(!uc@czat\\.wp\\.pl)"),
	  reFcuk_("([^\\s])<[biunc]=([^\\s])", boost::regex_constants::icase),
	  sent001_(0), sentBl_(0),
	  tcp_(io_service_, boost::bind(&ChatWP::handle_read, this), boost::bind(&ChatWP::handle_end, this), boost::bind(&ChatWP::handle_connect, this), "wp")
{
	WPConv::fill();
}

ChatWP::~ChatWP ()
{
	try {
		boost::filesystem::remove(tokenFile_);
	} catch (...) {}
	tcp_.close();
}

void ChatWP::init()
{
	int r = logIn();
	if (r) {
		end();
		return;
	}
}

void ChatWP::process(std::vector<std::string>& im)
{
	if (!im.size())
		return;

	boost::to_upper(im[0]);

	if (im[0] == "NICK" || im[0] == "PROTOCTL") {
		if (!sentBl_)
			sendInfo("NICK/PROTOCTL commands have been disabled. You won't be notified again.");
		sentBl_ = 1;
		return;
	}

	if (tokenNeeded_ && im.size() == 2 && im[0] == "TOKEN") {
		token_ = im[1];
		try {
			boost::filesystem::remove(tokenFile_);
		} catch (...) {}
		logIn();
		return;
	}
	else if (im[0] == "LIST") {
		sendInfo("");
		sendInfo("TODO (in near future! c; )");
		sendInfo("For the moment, use /SQUERY WPServ KATASK 0");
		sendInfo("");
	}
	else if (im[0] == "QUIT")
		if (im.size() > 1)
			im[1] += "\017 (\002chatproxy2\002)";
		else
			im.push_back("(\002chatproxy2\002)");
	else if (im[0] == "JOIN")
		im[0] = "WPJOIN";
	else if (im.size() >= 2 && (im[0] == "MODE" || im[0] == "WHO" || im[0] == "PRIVMSG" || im[0] == "NOTICE"
		|| im[0] == "INVITE" || im[0] == "WHOWAS"))
		im[1] = WPConv::hash(im[1]);
	else if (im.size() == 3 && im[0] == "WHOIS") {
		im[1] = WPConv::hash(im[1]);
		im[2] = WPConv::hash(im[2]);
	}
	else if (im.size() == 2 && (im[0] == "WHOIS"))
		im[1] = WPConv::hash(im[1]);
	else if (im.size() >= 3 && im[0] == "KICK")
		im[2] = WPConv::hash(im[2]);
	else if (im.size() >= 3 && im[0] == "SQUERY") {
		std::string serv(im[1]);
		boost::to_upper(serv);
		if (serv != "WPSERV")
			return;
		boost::regex reS("^(\\s*kick\\s+[^\\s]+\\s+)([^\\s]+)(.*)$", boost::regex_constants::icase);
		boost::smatch match;
		if (!boost::regex_match(im[2], match, reS))
			return;
		im[2] = match[1] + WPConv::hash(match[2]) + match[3];
	}

	WPConv::conv(0, 2, im);
	std::ostringstream x;
	Session::writeIM(x, im, (im[0] == "PRIVMSG" || im[0] == "NOTICE"));
	tcp_.write(x.str());
}



// ------------------ private ------------------ //



void ChatWP::handle_connect ()
{
	sendInfo("-!-   ... ok.");
	tcp_.write("NICK " + nickHash_ + "\r\n"
		+ "PASS " + Http::urlencode(ticket_) + "\r\n");
	tcp_.timeout(360);
	tcp_.readUntil("\n");
}

void ChatWP::handle_read ()
{
	boost::smatch match;
	bool doSend = 1;
	std::vector<std::string> im;
	std::string buf;
	if (tcp_.get(buf))
		return;
	WPConv::conv(2, 0, buf);
	std::istringstream is(buf);
	Session::readIM(is, im);

	if (im.size() >= 3) {
		boost::to_upper(im[1]);

		if (boost::regex_match(im[0], match, reUser_))
			im[0] = match[1] + WPConv::normal(match[2]) + match[3];
		else if (im[0] == ":" + nickHash_)
			im[0] = ":" + nick_;
		im[2] = WPConv::normal(im[2]);
	}

	if (im.size() == 4 && im[1] == "MAGIC") {
		sendInfo("-!-   ... server magic: " + im[3]);
		std::string m(magic(im[3]));
		sendInfo("-!-   ... recoded: " + m);
		sendInfo("-!-");
		tcp_.write("USER " + tcp_.socket_.local_endpoint().address().to_string()
			+ " 8 " + m + " :" + realname_
			+ "\017 (\002chatproxy2\002)\r\n");
		doSend = 0;
	}
	else if (im.size() >= 3 && im[1] == "WPJOIN")
		im[1] = "JOIN";
	else if (!sent001_ && im.size() >= 2 && im[1] == "375") {
		sendRaw(":chatproxy2 001 " + nick_ + " :(fake) Welcome.\r\n");
		sent001_ = 1;
	}
	else if (im.size() >= 2 && im[1] == "376") {
		sendInfo("");
		sendInfo("WP module hints:");
		sendInfo("");
		sendInfo("  1) set /TOKEN alias, to easily provide cp2 with token:");
		sendInfo("           /alias TOKEN QUOTE TOKEN $-");
		sendInfo("  2) use WPServ to try to hack sth:");
		sendInfo("           /SQUERY WPServ HELP");
		sendInfo("           /SQUERY WPServ PYTANIE #rozmowy Hello?");
		sendInfo("");
		if (!adm())
			tcp_.write("WPJOIN #chatproxy\r\n");
	}
	else if (im.size() >= 4 && im[1] == "PART")
		im[im.size() - 1] = WPConv::normal(im[im.size() - 1]);
	else if (im.size() == 6 && im[1] == "353") {
		std::vector<std::string> nicks;
		std::istringstream nis(im[5]);
		Session::readIM(nis, nicks);
		im[5] = "";
		for (size_t i = 0; i < nicks.size(); i++)
			im[5] += (i ? " " : "")
				+ ((nicks[i][0] == '@' || nicks[i][0] == '+') ? std::string(1, nicks[i][0]) : std::string())
				+ WPConv::normal((nicks[i][0] == '@' || nicks[i][0] == '+') ? nicks[i].substr(1) : nicks[i]);
	}
	else if (im.size() >= 8 && im[1] == "352")
		im[7] = WPConv::normal(im[7]);
	else if (im.size() >= 4 && im[1] == "KICK")
		im[3] = WPConv::normal(im[3]);
	else if (im.size() >= 4 && im[1] == "MODE")
		for (size_t i = 3; i < im.size(); i++)
			im[i] = WPConv::normal(im[i]);
	else if (im.size() >= 4 && (im[1][0] == '4' || im[1][0] == '3'))
		im[3] = WPConv::normal(im[3]);

	if (doSend) {
		im[im.size() - 1] = boost::regex_replace(im[im.size() - 1], reCut_, "");
		im[im.size() - 1] = boost::regex_replace(im[im.size() - 1], reFcuk_, "${1}${2}");
		send(im, (im[1] == "PRIVMSG" || im[1] == "NOTICE"));
	}

	tcp_.readUntil("\n");
}

void ChatWP::handle_end ()
{
	end();
}

int ChatWP::logIn ()
{
	sendInfo("-!- ");

	if (username_.empty()) {
		if (!tokenNeeded_) {
			sendInfo("-!- Getting token image...");

			if (ht_.get("http://czat.wp.pl/chat.html")) {
				sendInfo("-!-   ... failed (-1, " + ht_.error() + ").");
				return -1;
			}

			boost::regex reToken("\\s+src=\"(http://si.wp.pl/obrazek[^\"]*)");
			boost::smatch match;
			if (!boost::regex_search(ht_.result(), match, reToken)) {
				sendInfo("-!-   ... failed (-2, No token @ chat.html).");
				return -2;
			}

			boost::tuple<boost::filesystem::path, std::string> token = genToken();
			tokenFile_ = boost::get<0>(token);
			tokenFile_.replace_extension("png");

			ht_.file(tokenFile_.string());
			int r = ht_.get(match[1]);
			ht_.file();

			if (r) {
				sendInfo("-!-   ... failed (-3, " + ht_.error() + ").");
				return -3;
			}

			sendInfo("-!-   ... ok. Read the image @ " + boost::get<1>(token) + ".png");
			sendInfo("-!-        and provide it typing: /quote TOKEN {read_token_string}");

			tokenNeeded_ = 1;
			return 0;
		}
		else {
			sendInfo("-!- Sending token, getting magic...");

			if (ht_.get("http://czat.wp.pl/chat.html?i=1&auth=nie&nick=" + Http::urlencode(nick_) + "&simg=" + Http::urlencode(token_) + "&x=0&y=0")) {
				sendInfo("-!-   ... failed (-4, " + ht_.error() + ").");
				return -4;
			}

			if (ht_.result().find("document.logowanie.nick.focus") != std::string::npos) {
				try {
					boost::filesystem::remove(tokenFile_);
				} catch (...) {}
				sendInfo("-!-   ... esh, you've misread the image... once again.");
				tokenNeeded_ = 0;
				token_ = "";
				return logIn();
			}

			boost::regex reMagic("<param\\s+name=\"magic\"\\s+value=\"([^\"]+)");
			boost::smatch match;
			if (!boost::regex_search(ht_.result(), match, reMagic)) {
				sendInfo("-!-   ... failed (-5, No magic).");
				return -5;
			}

			tokenNeeded_ = 0;
			magic_ = match[1];
			sendInfo("-!-   ... ok: " + magic_);

			nick_.insert(0, "~");
			nickHash_ = WPConv::hash(nick_);
			sendInfo("-!-");
			sendInfo("-!- Getting ticket...");
			if (ht_.get("http://czati1.wp.pl/getticket.html?nick=" + Http::urlencode(nick_))) {
				sendInfo("-!-   ... failed (-6, " + ht_.error() + ").");
				return -6;
			}

			ticket_ = ht_.result();
			boost::trim(ticket_);
			sendInfo("-!-   ... ok: " + ticket_);
			sendInfo("-!-");
			sendInfo("-!- Connecting...");

			tcp_.timeout(10);
			tcp_.connect("czati1.wp.pl", 5579);

			return 0;
		}
	}
	else {
		sendInfo("-!- Logging in...");

		if (ht_.get("http://profil.wp.pl/index2.html")) {
			sendInfo("-!-   ... failed (-7, " + ht_.error() + ").");
			return -7;
		}

		if (ht_.post("http://profil.wp.pl/index2.html", "serwis=wp.pl&url=profil.html&tryLogin=1&countTest=1&login_username="
			+ Http::urlencode(username_) + "&login_password=" + Http::urlencode(password_)
			+ "&savelogin=2&savessl=2&starapoczta=2&minipoczta=2&zaloguj=Zaloguj")) {
			sendInfo("-!-   ... failed (-8, " + ht_.error() + ").");
			return -8;
		}

		if (ht_.result().find("f.savelogin.value") != std::string::npos) {
			sendInfo("-!-   ... invalid username and/or password.");
			return -9;
		}

		sendInfo("-!-   ... ok.");
		sendInfo("-!-");
		sendInfo("-!- Getting magic...");

		if (ht_.get("http://czat.wp.pl/auth,tak,i,1,nick," + Http::urlencode(nick_) + ",chat.html")) {
			sendInfo("-!-   ... failed (-10, " + ht_.error() + ").");
			return -10;
		}

		if (ht_.result().find("document.logowanie.nick.focus") != std::string::npos) {
			sendInfo("-!-   ... you do not own that nick!");
			return -11;
		}

		boost::regex reMagic("<param\\s+name=\"magic\"\\s+value=\"([^\"]+)");
		boost::smatch match;
		if (!boost::regex_search(ht_.result(), match, reMagic)) {
			sendInfo("-!-   ... failed (-12, No magic).");
			return -12;
		}

		magic_ = match[1];
		sendInfo("-!-   ... ok: " + magic_);

		nickHash_ = WPConv::hash(nick_);
		sendInfo("-!-");
		sendInfo("-!- Getting ticket...");
		if (ht_.get("http://czati1.wp.pl/getticket.html?nick=" + Http::urlencode(nick_))) {
			sendInfo("-!-   ... failed (-13, " + ht_.error() + ").");
			return -13;
		}

		ticket_ = ht_.result();
		boost::trim(ticket_);
		sendInfo("-!-   ... ok: " + ticket_);
		sendInfo("-!-");
		sendInfo("-!- Connecting...");

		tcp_.timeout(10);
		tcp_.connect("czati1.wp.pl", 5579);
	}

	return 0;
}

std::string ChatWP::magic (const std::string& smagic)
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

	const std::string& s2 = magic_, s1 = smagic;

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
		// ;) that was kewl:
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

	struct timeval tv;
	gettimeofday(&tv, 0);
	long long unsigned int l1 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);

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

namespace WPConv
{
	bool filled = 0;
	char q[256], u[256], w[256], A[256], E[256];
	boost::regex reHash("^([ab])([^|]+).(.+)$");

void fill ()
{
	if (filled)
		return;
	filled = 1;

	for(int i1 = 0; i1 < 256; i1++) {
		E[i1] = '_';
		u[i1] = '_';
		A[i1] = '_';
		q[i1] = '_';
		w[i1] = 1;
	}

	//std::string ac("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789±æê³ñó¶¿¼¡ÆÊ£ÑÓ¦¯¬[]_-^/:;?.,=)!@#$%+&*(");
	std::string ac("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789±æê³ñó¶¿¼¡ÆÊ£ÑÓ¦¯¬[]_-^/:;?.,=)!|@#$%+&*(");

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

	E[(unsigned char)'¡'] = '±';
	E[(unsigned char)'Æ'] = 'æ';
	E[(unsigned char)'Ê'] = 'ê';
	E[(unsigned char)'£'] = '³';
	E[(unsigned char)'Ñ'] = 'ñ';
	E[(unsigned char)'Ó'] = 'ó';
	E[(unsigned char)'¦'] = '¶';
	E[(unsigned char)'¯'] = '¿';
	E[(unsigned char)'¬'] = '¼';
	for(int i2 = 97; i2 <= 122; i2++)
		u[i2] = std::toupper((char )i2);
	u[(unsigned char)'±'] = '¡';
	u[(unsigned char)'æ'] = 'Æ';
	u[(unsigned char)'ê'] = 'Ê';
	u[(unsigned char)'³'] = '£';
	u[(unsigned char)'ñ'] = 'Ñ';
	u[(unsigned char)'ó'] = 'Ó';
	u[(unsigned char)'¶'] = '¦';
	u[(unsigned char)'¿'] = '¯';
	u[(unsigned char)'¼'] = '¬';
	A[(unsigned char)'±'] = 'a';
	A[47] = 'b';
	A[(unsigned char)'æ'] = 'c';
	A[58] = 'd';
	A[(unsigned char)'ê'] = 'e';
	A[59] = 'f';
	A[(unsigned char)'³'] = 'l';
	A[63] = 'g';
	A[46] = 'k';
	A[(unsigned char)'ñ'] = 'n';
	A[(unsigned char)'ó'] = 'o';
	A[44] = 'p';
	A[61] = 'r';
	A[(unsigned char)'¶'] = 's';
	A[(unsigned char)'¿'] = 'z';
	A[(unsigned char)'¼'] = 'x';
	A[41] = '0';
	A[33] = '1';
	A['|'] = '1';
	A[64] = '2';
	A[35] = '3';
	A[36] = '4';
	A[37] = '5';
	A[43] = '6';
	A[38] = '7';
	A[42] = '8';
	A[40] = '9';
	q[97] = '±';
	q[98] = '/';
	q[99] = 'æ';
	q[100] = ':';
	q[101] = 'ê';
	q[102] = ';';
	q[108] = '³';
	q[103] = '?';
	q[107] = '.';
	q[110] = 'ñ';
	q[111] = 'ó';
	q[112] = ',';
	q[114] = '=';
	q[115] = '¶';
	q[122] = '¿';
	q[120] = '¼';
	q[48] = ')';
	// q[49] = '!';
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

std::string normal (const std::string& s)
{
	boost::smatch match;
	if (!boost::regex_match(s, match, reHash))
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

std::string hash (const std::string& s)
{
	// prevent #chans from getting hashed
	// possible flaw when there's a person
	// whose nick starts with '#'... :p
	if (s[0] == '#' || boost::regex_match(s, boost::regex("^(operator_?|straznik|WK-bot[0-9]+)$")))
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

void conv (size_t from, size_t to, std::vector<std::string>& s)
{
	for (size_t i = 0; i < s.size(); i++)
		conv(from, to, s[i]);
}

void conv (size_t from, size_t to, std::string& s)
{
	for (size_t i = 0; i < s.size(); i++)
		s[i] = conv(from, to, s[i]);
}

/*
 * conv() (c) Adam Wysocki <gophi ma³pa chmurka.net>, www.gophi.rotfl.pl
 *
 * enum {
 * 	CP_ISO = 0,	// ISO-8859-2
 * 	CP_IBM,		// IBM-852 (MS-DOS)
 * 	CP_WIN		// CP-1250 (MS-Windows)
 * };
 */

char conv (size_t from, size_t to, char ch)
{
	const static char ctab[3][19] = {
		{ 0xEA, 0xF3, 0xB1, 0xB6, 0xB3, 0xBF, 0xBC, 0xE6, 0xF1, 0xCA, 0xD3, 0xA1, 0xA6, 0xA3, 0xAF, 0xAC, 0xC6, 0xD1, 0 },
		{ 0xA9, 0xA2, 0xA5, 0x98, 0x88, 0xBE, 0xAB, 0x86, 0xE4, 0xA8, 0xE0, 0xA4, 0x98, 0x9D, 0xBD, 0x8D, 0x8F, 0xE3, 0 },
		{ 0xEA, 0xF3, 0xB9, 0x9C, 0xB3, 0xBF, 0x9F, 0xE6, 0xF1, 0xCA, 0xD3, 0xA5, 0x8C, 0xA3, 0xAF, 0x8F, 0xC6, 0xD1, 0 }
	};
	size_t i = 0;

	while (ctab[from][i])
		if (ch == ctab[from][i])
			return ctab[to][i];
		else
			i++;

	return ch;
}

};
