#include "ChatOnet.h"

#include <sstream>
#include <algorithm>

#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>

#include "EException.h"
#include "Session.h"
#include "Http.h"
#include "Tcp.h"

// 16:14:55 [OnetJodua] -!- MODERATE jodua ~deem #Admin cf1f35bc54abcd05f :co¶ innego.

ChatOnet::ChatOnet (Session* session)
	: Chat(session),
	  reModC_("%C[0-9a-f]+%"), reModF_("%F[0-9a-z:]+%"), reModI_("%I([0-9A-Za-z_]+)%"),
	  sentBl_(0),
	  tcp_(io_service_, boost::bind(&ChatOnet::handle_read, this), boost::bind(&ChatOnet::handle_end, this), boost::bind(&ChatOnet::handle_connect, this), "onet")
{
}

ChatOnet::~ChatOnet ()
{
	tcp_.close();
}

void ChatOnet::init()
{
	sendInfo("-!- ");
	sendInfo("-!- Getting UO key ...");

	int r;
	if ((r = getUoKey())) {
		sendInfo("-!-   ... failed (" + boost::lexical_cast<std::string>(r) + ").");
		end();
		return;
	}

	sendInfo("-!-   ... " + uoKey_);
	sendInfo("-!-   ... " + nick_);
	sendInfo("-!- ");
	sendInfo("-!- Connecting ...");
	sendInfo("-!- ");

	tcp_.timeout(10);
	tcp_.connect("czat-app.onet.pl", 5015);
}

void ChatOnet::process(std::vector<std::string>& im)
{
	if (!im.size())
		return;

	boost::to_upper(im[0]);

	if (im.size() == 2 && im[0] == "MODE" && (im[1][0] == '#' || im[1][0] == '+')) {
		sendRaw(":chatproxy2 324 " + nick_ + " " + im[1] + " +\r\n");
		return;
	}
	else if (im.size() == 2 && im[0] == "WHO" && (im[1][0] == '#' || im[1][0] == '+')) {
		sendRaw(":chatproxy2 315 " + nick_ + " " + im[1] + " :End of WHO list.\r\n");
		return;
	}
	else if (im.size() == 3 && im[0] == "MODE" && (im[1][0] == '#' || im[1][0] == '+') && im[2] == "b") {
		sendRaw(":chatproxy2 368 " + nick_ + " " + im[1] + " :End of Channel Ban List\r\n");
		return;
	}

	if (im.size() >= 2 && im[1][0] == '+')
		im[1][0] = '^';

	if (im[0] == "MODE" || im[0] == "WHO" || im[0] == "NICK" || im[0] == "PROTOCTL") {
		if (!sentBl_)
			sendInfo("MODE/WHO/NICK/PROTOCTL commands have been disabled. You won't be notified again.");
		sentBl_ = 1;
		return;
	}

	if (im[0] == "LIST")
		im[0] = "SLIST";
	else if (im.size() >= 3 && im[0] == "MODERNOTICE")
		sendRaw(":" + nick_ + "!fake@chatproxy2 NOTICE " + im[1] + " :" + im[2] + "\r\n");
	else if (im.size() >= 5 && im[0] == "MODERMSG")
		sendRaw(":\00314" + nick_ + "\003\002:\002" + im[1] + "!fake@chatproxy2 PRIVMSG "
			+ im[3] + " :" + im[4] + "\r\n");
	else if (im[0] == "QUIT") {
		if (im.size() > 1)
			im[1] += "\017 (\002chatproxy2\002)";
		else
			im.push_back("(\002chatproxy2\002)");
	}

	std::ostringstream x;
	Session::writeIM(x, im, (im[0] == "PRIVMSG" || im[0] == "NOTICE"));
	tcp_.write(x.str());
}



// ------------------ private ------------------ //



void ChatOnet::handle_connect ()
{
	tcp_.write("AUTHKEY\r\n");
	tcp_.timeout(360);
	tcp_.readUntil("\n");
}

void ChatOnet::handle_read ()
{
	bool doSend = 1;
	std::vector<std::string> im;
	std::string buf;
	if (tcp_.get(buf))
		return;
	std::istringstream is(buf);
	Session::readIM(is, im);

	if (im.size() >= 3)
		boost::to_upper(im[1]);

	size_t n = im.size();
	if (n && !(n >= 2 && im.size() >= 2 && (im[1] == "INVITE" || im[1] == "PART" || im[1] == "341"))) n--;
	for (size_t i = 2; i < n; i++) if (im[i][0] == '^') im[i][0] = '+';

	if (im.size() >= 4 && im[1] == "801") {
		sendInfo("-!- ");
		sendInfo("-!-   ... authkey: " + im[3]);
		std::string auth = authkey(im[3]);
		sendInfo("-!-   ... recoded: " + auth);
		sendInfo("-!- ");

		tcp_.write("NICK " + nick_ + "\r\n"
			+ "AUTHKEY " + auth + "\r\n"
			+ "USER * " + uoKey_ + " czat-app.onet.pl :" + realname_
			+ "\017 (\002chatproxy2\002)\r\n");
		doSend = 0;
	}
	else if (im.size() >= 4 && im[1] == "005") {
		for (size_t i = 3; i < im.size(); i++)
			if (im[i] == "CHANTYPES=^#") {
				im[i] = "CHANTYPES=#+";
				break;
			}
	}
	else if (im.size() >= 4 && im[1] == "MODE" && (im[3] == "-W" || im[3] == "+W" || im[3] == "+b" || im[3] == "-b"))
		doSend = 0;
	else if (im.size() >= 8 && im[1] == "817") {
		std::vector<std::string> nim;
		nim.push_back(":history");
		nim.push_back("NOTICE");
		nim.push_back(im[3]);
		nim.push_back(epochToHuman(boost::lexical_cast<unsigned int>(im[4]), 1) + " <" + im[5] + "> " + im[7]);
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
		sendInfo("");
		sendInfo("Onet module hints:");
		sendInfo("");
		sendInfo("  1) set /PRIV alias, to invite others to privs:");
		sendInfo("           /alias PRIV QUOTE PRIV $-");
		sendInfo("");
		if (!adm())
			tcp_.write("JOIN #chatproxy\r\n");
	}
	else if (im.size() >= 2 && im[1] == "818") {
		list_.clear();
		doSend = 0;
	}
	else if (im.size() >= 4 && im[1] == "819") {
		std::vector<std::string> x;
		Channel tmp;
		boost::split(x, im[3], boost::is_any_of(":,"), boost::token_compress_off);
		for (size_t i = 0; i < x.size(); i += 3) {
			tmp.name = x[i];
			tmp.topic = x[i + 1];
			tmp.num = boost::lexical_cast<size_t>(x[i + 2]);
			list_.push_back(tmp);
		}
		doSend = 0;
	}
	else if (im.size() >= 2 && im[1] == "820") {
		std::sort(list_.begin(), list_.end());
		for (size_t i = 0; i < list_.size(); i++)
			sendRaw(":chatproxy2 322 " + nick_ + " " + list_[i].name + " "
				+ boost::lexical_cast<std::string>(list_[i].num) + " :"
				+ list_[i].topic + "\r\n");
		sendRaw(":chatproxy2 323 " + nick_ + " :End of LIST\r\n");
		doSend = 0;
	}
	else if (im.size() >= 4 && im[1] == "NOTICE" && im[0] == ":ChanServ!service@service.onet") {
		boost::regex reEpoch("[1-9][0-9]{7}[0-9]+");
		boost::smatch match;
		while (boost::regex_search(im[im.size() - 1], match, reEpoch))
			boost::replace_all(im[im.size() - 1], std::string(match[0]), "\"" + epochToHuman(boost::lexical_cast<unsigned int>(match[0])) + "\"");
	}

	if (doSend) {
		size_t last = im.size() - 1;
		im[last] = boost::regex_replace(im[last], reModC_, "");
		im[last] = boost::regex_replace(im[last], reModF_, "");
		im[last] = boost::regex_replace(im[last], reModI_, "<$1>");

		size_t p = -1;
		while ((p = im[last].find("%%", ++p)) != std::string::npos)
			im[last].replace(p, 2, "%");

		send(im, (im[1] == "PRIVMSG" || im[1] == "NOTICE"));
	}

	tcp_.readUntil("\n");
}

void ChatOnet::handle_end ()
{
	end();
}

int ChatOnet::getUoKey ()
{
	Http ht;
	std::stringstream dat;

	if (password_.empty()) {
		dat << "api_function=getUoKey&params=a:3:{s:4:\"nick\";s:" << nick_.size() << ":\"" << nick_
			<< "\";s:8:\"tempNick\";i:1;s:7:\"version\";s:3:\"ksz\";}";
		if (ht.post("http://czat.onet.pl/include/ajaxapi.xml.php3", dat.str()))
			return -2;
	}
	else {
		if (ht.get("http://czat.onet.pl/_s/kropka/1?DV=Incognito"))
			return -1;
		if (ht.get("http://secure.onet.pl/index.html"))
			return -3;
		if (ht.post("https://secure.onet.pl/index.html", "r=&url=&login=" + Http::urlencode(nick_)
				+ "&haslo=" + Http::urlencode(password_) + "&ok=Ok"))
			return -4;
		dat << "api_function=getUoKey&params=a:3:{s:4:\"nick\";s:" << nick_.size() << ":\"" << nick_
			<< "\";s:8:\"tempNick\";i:0;s:7:\"version\";s:3:\"ksz\";}";
		if (ht.post("http://czat.onet.pl/include/ajaxapi.xml.php3", dat.str()))
			return -5;
	}

	std::string r = ht.result();
	boost::regex reUo("<uoKey>([^<]+)");
	boost::regex reZuo("<zuoUsername>([^<]+)");
	boost::smatch match;

	if (!boost::regex_search(r, match, reUo))
		return -6;
	uoKey_ = match[1];

	if (!boost::regex_search(r, match, reZuo))
		return -7;
	nick_ = match[1];

	return 0;
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
