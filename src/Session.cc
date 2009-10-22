#include "Session.h"

#include <iomanip>
#include <sstream>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <GeoIP.h>

#include "Config.h"

#include "Chat.h"
#include "ChatOnet.h"
#include "ChatWP.h"

Session::Session (Config& cnf, boost::asio::io_service& io_service)
	: io_service_(io_service), cnf_(cnf), startTime_(time(0)), deletable_(0),
	  authGotNick_(0), authGotUser_(0), authGotPass_(0),
      identReady_(0),
	  tcp_(io_service, boost::bind(&Session::handle_read, this), boost::bind(&Session::handle_end, this), boost::function<void()>(), "session"),
	  tcpIdent_(io_service, boost::bind(&Session::ident_handle_read, this), boost::bind(&Session::ident_handle_end, this),
		boost::bind(&Session::ident_handle_connect, this), "ident")
{
}

Session::~Session ()
{
}

void Session::start ()
{
	try {
		myHost_ = tcp_.socket_.local_endpoint().address().to_string();
		myPort_ = boost::lexical_cast<std::string>(tcp_.socket_.local_endpoint().port());
		remoteHost_ = tcp_.socket_.remote_endpoint().address().to_string();
		remotePort_ = boost::lexical_cast<std::string>(tcp_.socket_.remote_endpoint().port());

		GeoIP* gi;
		gi = GeoIP_new(GEOIP_STANDARD);
		const char* tmp = GeoIP_country_code_by_addr(gi, remoteHost_.c_str());
		remoteCC_ = (tmp ? tmp : "--");
		GeoIP_delete(gi);
	}
	catch (...) {
		end();
	}

	tcpIdent_.timeout(5);
	tcpIdent_.connect(remoteHost_, 113);

	tcp_.timeout(5);
	tcp_.readUntil("\n");
	sendRaw(":chatproxy2 020 * :Please wait while we process your connection.\r\n");
}

void Session::end (const std::string& reason)
{
	if (!reason.empty())
		sendInfo(reason);
	tcp_.close();
}

void Session::readIM (std::istream& is, std::vector<std::string>& im)
{
	im.clear();

	std::string tmp;
	std::getline(is, tmp);
	if (tmp[tmp.size() - 1] == '\r')
		tmp.erase(tmp.size() - 1);

	std::istringstream iss(tmp);

	while (!(iss >> std::ws).eof()) {
		if (!im.empty() && iss.peek() == ':')
			std::getline(iss.ignore(1), tmp);
		else
			iss >> std::noskipws >> tmp;
		im.push_back(tmp);
	}
}

void Session::writeIM (std::ostream& os, const std::vector<std::string>& im, bool lastText)
{
	for (size_t i = 0; i < im.size(); i++) {
		if (i)
			os << " ";
		if (i == im.size() - 1 && (lastText || im[i].empty() || im[i].find(' ') != std::string::npos))
			os << ":";
		os << im[i];
	}
	os << "\r\n";
}

void Session::send (const std::vector<std::string>& im, bool lastText)
{
	std::ostringstream x;
	writeIM(x, im, lastText);
	sendRaw(x.str());
}

void Session::sendInfo (const std::string& msg, const std::string& to)
{
	sendRaw(":chatproxy2 NOTICE " + to + " :" + msg + "\r\n");
}

void Session::sendAdm (const std::string& msg)
{
	sendRaw(":&chatproxy2 PRIVMSG " + nick_ + " :" + msg + "\r\n");
}

void Session::sendRaw (const std::string& data)
{
	tcp_.write(data);
}

std::string Session::epochToHuman (unsigned int epoch, bool onlyHMS)
{
	time_t tme = epoch;
	struct tm tv;
	localtime_r(&tme, &tv);
	std::stringstream stme;
	stme << std::setfill('0');
	if (!onlyHMS)
		stme << std::setw(4) << (1900 + tv.tm_year) << std::setw(2) << (tv.tm_mon + 1) << std::setw(2) << tv.tm_mday << ".";
	stme << std::setw(2) << tv.tm_hour << std::setw(2) << tv.tm_min << std::setw(2) << tv.tm_sec;
	return stme.str();
}



// ------------------ private ------------------ //



void Session::handle_read ()
{
	std::vector<std::string> im;
	std::string buf;
	if (tcp_.get(buf))
		return;
	std::istringstream is(buf);
	readIM(is, im);

	if (!im.empty())
		boost::to_upper(im[0]);

	if (!user_)
		auth(im);
	else if (im.size() == 3 && im[0] == "PRIVMSG" && im[1] == "&chatproxy2")
		adm(im[2]);
	else if (chat_)
		chat_->process(im);
	// else: ending phase

	tcp_.readUntil("\n");
}

void Session::handle_end ()
{
	if (chat_)
		chat_ = boost::shared_ptr<Chat>();
	deletable_ = 1;
	io_service_.post(boost::bind(&Config::sessionsCleanup, &cnf_));
}

void Session::ident_handle_connect ()
{
	tcpIdent_.write(remotePort_ + " , " + myPort_ + "\r\n");
	tcpIdent_.readUntil("\n");
}

void Session::ident_handle_end ()
{
	{
		boost::lock_guard<boost::mutex> lock(identMutex_);
		identReady_ = 1;
	}
	identCond_.notify_all();
}

void Session::ident_handle_read ()
{
	boost::smatch match;
	boost::regex reIdent("\\s*[0-9]+\\s*,\\s*[0-9]+\\s*:\\s*[^\\s]+\\s*:\\s*[^\\s]+\\s*:\\s*([^\\s]+)\\s*");
	std::string buf;

	if (!tcpIdent_.get(buf))
		if (boost::regex_match(buf, match, reIdent))
			ident_ = match[1];

	tcpIdent_.readUntil("\n");
	tcpIdent_.close();
}

void Session::auth (std::vector<std::string>& im)
{
	bool ok = 1;
	size_t p;

	if (im.size())
		boost::to_upper(im[0]);

	if (im.size() < 2)
		ok = 0;
	else if (im[0] == "PASS") {
		if ((p = im[1].find(':')) == std::string::npos)
			ok = 0;
		else {
			authUser_ = im[1].substr(0, p);
			authPass_ = im[1].substr(p + 1);
		}
		authGotPass_ = 1;
	}
	else if (im[0] == "NICK") {
		if ((p = im[1].find('@')) == std::string::npos)
			ok = 0;
		else {
			nick_ = im[1].substr(0, p);
			network_ = im[1].substr(p + 1);
			boost::to_lower(network_);

			if ((p = nick_.find(':')) != std::string::npos) {
				password_ = nick_.substr(p + 1);
				nick_ = nick_.substr(0, p);

				if ((p = password_.find(':')) != std::string::npos) {
					username_ = password_.substr(0, p);
					password_ = password_.substr(p + 1);
				}
			}
		}
		authGotNick_ = 1;
	}
	else if (im[0] == "USER") {
		if (im.size() == 5) {
			realname_ = im[4];
			if (ident_.empty())
				ident_ = "~" + im[1];
		}
		else
			ok = 0;
		authGotUser_ = 1;
	}
	else
		ok = 0;

	if (ok && authGotUser_ && authGotNick_ && !authGotPass_) {
		authPass_ = cnf_.upassword("visitor");
		if (!authPass_.empty()) {
			authUser_ = "visitor";
			authGotPass_ = 1;
		}
	}

	if (!ok || (authGotUser_ && authGotNick_ && !authGotPass_)) {
		sendInfo("");
		sendInfo("chatproxy2 (c) 2004-* Michal Rus");
		sendInfo("  vel miszka / jodua <m@michalrus.com>");
		sendInfo("");
		sendInfo("To connect validly, your client has to send:");
		sendInfo("");
		sendInfo("  PASS {user}:{password}");
		sendInfo("  NICK {nick}[[:{username}]{password}]@{network}");
		sendInfo("  USER {} {} {} :{real}");
		sendInfo("");
		sendInfo("WTF?! Well, the key:");
		sendInfo("");
		sendInfo("  {user}:{password}");
		sendInfo("      credentials *I* gave you");
		sendInfo("  {nick}  -OR-  {nick}:{password}");
		sendInfo("          -OR-  {nick}:{username}:{password}");
		sendInfo("      credentials required by the network");
		sendInfo("  {network}");
		sendInfo("      onet / wp");
		sendInfo("  {}");
		sendInfo("      whatever");
		sendInfo("  {real}");
		sendInfo("      your real(?) name");
		sendInfo("");
		sendInfo("In Irssi this can be done using:");
		sendInfo("");
		sendInfo("  /set real_name {real}");
		sendInfo("  /connect " + myHost_ + " " + myPort_ + " {user}:{password}");
		sendInfo("      {nick}[[:{username}]{password}]@{network}");
		sendInfo("");
		sendInfo("Your connection will now be terminated . . .");
		sendInfo("");
		sendInfo("                                 -- miszka <33");
		sendInfo("");

		end();
		return;
	}

	if (!authGotUser_ || !authGotNick_)
		return;

	boost::shared_ptr<User> user;
	user = cnf_.authenticate(authUser_, authPass_);

	if (!user) {
		sendInfo("");
		sendInfo("Local auth fail. Contact me if you want to have access.");
		sendInfo("");
		sendInfo("  /connect " + myHost_ + " " + myPort_);
		sendInfo("");

		end();
		return;
	}

	{
		boost::unique_lock<boost::mutex> lock(identMutex_);
		while (!identReady_)
			identCond_.wait(lock);
	}

	bool accessOK_ = 1;
	{
		boost::shared_lock<boost::shared_mutex> lock(cnf_.accessMutex_);
		std::string mask = ident_ + "@" + remoteCC_ + ":" + remoteHost_;

		for (size_t i = 0; i < cnf_.getAccess().size(); i++)
			if (cnf_.getAccess()[cnf_.getAccess().size() - 1 - i].match(mask)) {
				accessOK_ = cnf_.getAccess()[cnf_.getAccess().size() - 1 - i].allow();
				break;
			}
	}

	if (!accessOK_ && !user->adm) {
		sendInfo("");
		sendInfo("Hi, " + user->name + ", how are you? Seems you're banned. ;O");
		sendInfo("Contact me to clarify teh situation.");
		sendInfo("");
		sendInfo("  /connect " + myHost_ + " " + myPort_);
		sendInfo("");

		end();
		return;
	}

	size_t sAll = 0, sIP = 0;
	{
		boost::shared_lock<boost::shared_mutex> lock(cnf_.sessionsMutex_);
		for (size_t i = 0; i < cnf_.getSessions().size(); i++)
			if (cnf_.getSessions()[i]->user_ == user) {
				sAll++;
				if (cnf_.getSessions()[i]->remoteHost_ == remoteHost_ && cnf_.getSessions()[i]->network_ == network_)
					if ((ident_[0] == '~' && cnf_.getSessions()[i]->ident_[0] == '~')
						|| ident_ == cnf_.getSessions()[i]->ident_)
						sIP++;
			}
	}

	if (sAll >= user->sessionNumMax) {
		sendInfo("");
		sendInfo("Hi, " + user->name + ", how are you? Seems you've reached your session");
		sendInfo("number limit. Contact me if you need *moar* sessions.");
		sendInfo("");
		sendInfo("  /connect " + myHost_ + " " + myPort_);
		sendInfo("");

		end();
		return;
	}

	if (sIP >= user->sessionNumIPMax) {
		sendInfo("");
		sendInfo("Hi, " + user->name + ", how are you? Seems you've reached your session");
		sendInfo("number limit (on single ip). Contact me if you need *moar*");
		sendInfo("sessions.");
		sendInfo("");
		sendInfo("  /connect " + myHost_ + " " + myPort_);
		sendInfo("");

		end();
		return;
	}

	if (network_ == "onet")
		chat_ = boost::shared_ptr<Chat>(new ChatOnet(this));
	else if (network_ == "wp")
		chat_ = boost::shared_ptr<Chat>(new ChatWP(this));

	if (!chat_) {
		sendInfo("");
		sendInfo("Unsupported network. For help, connect without params:");
		sendInfo("");
		sendInfo("  /connect " + myHost_ + " " + myPort_);
		sendInfo("");

		end();
		return;
	}

	sendInfo("");
	sendInfo("Hi, " + user->name + ", how are you? Get ready for a ride!");
	sendInfo("");

	tcp_.timeout(360);
	chat_->init();

	user_ = user;
}

void Session::adm (const std::string& command)
{
	if (!user_->adm) {
		sendAdm("Permission denied.");
		return;
	}

	boost::regex reDie       ("\\s*die\\s*");
	boost::regex reHelp      ("\\s*help\\s*");
	boost::regex reLsUsr     ("\\s*lsusr\\s*");
	boost::regex reLsConn    ("\\s*lsconn\\s*");
	boost::regex reLsAccess  ("\\s*lsfilt\\s*");
	boost::regex reAccessAdd ("\\s*filtadd\\s+(allow|deny)\\s+([^\\s]+)\\s*");
	boost::regex reAccessDel ("\\s*filtdel\\s+(allow|deny)\\s+([^\\s]+)\\s*");
	boost::regex reUserAdd   ("\\s*useradd\\s+([^\\s]+)\\s+([^\\s]+)\\s*");
	boost::regex reUserDel   ("\\s*userdel\\s+([^\\s]+)\\s*");
	boost::regex reUserModPw ("\\s*usermod\\s+([^\\s]+)\\s+pw\\s+([^\\s]+)\\s*");
	boost::regex reUserModMax("\\s*usermod\\s+([^\\s]+)\\s+max\\s+([1-9][0-9]{0,3})\\s*");
	boost::regex reUserModIP ("\\s*usermod\\s+([^\\s]+)\\s+ip\\s+([1-9][0-9]{0,3})\\s*");
	boost::regex reUserModAdm("\\s*usermod\\s+([^\\s]+)\\s+adm\\s+([01])\\s*");
	boost::smatch match;

	if (boost::regex_match(command, reDie))
		std::exit(-2);
	else if (boost::regex_match(command, reHelp)) {
		sendAdm("Available commands:");
		sendAdm("  - die         -> commit, like, suicide");
		sendAdm("  - help         -> display this message");
		sendAdm("  - lsusr         -> list all users");
		sendAdm("  - lsconn         -> list all connections");
		sendAdm("  - lsfilt          -> list filter rules");
		sendAdm("  - filtadd {p} {m}  -> add rule ({p}: allow/deny, {m}: mask)");
		sendAdm("  - filtdel {p} {m}   -> del rule");
		sendAdm("  - useradd {u} {p}    -> add {u} and set his password to {p}");
		sendAdm("  - userdel {u}         -> kill {u}'s connections and delete him");
		sendAdm("  - usermod {u} pw {p}   -> set {u}'s password to {p}");
		sendAdm("  - usermod {u} max {i}  -> set {u}'s session limit to {i}");
		sendAdm("  - usermod {u} ip {i}   -> set {u}'s single ip limit to {i}");
		sendAdm("  - usermod {u} adm {b} -> set {u}'s adm flag to {b} (0/1)");
	}
	else if (boost::regex_match(command, reLsUsr)) {
		std::vector<std::string> x;
		{
			boost::shared_lock<boost::shared_mutex> lock(cnf_.usersMutex_);
			for (size_t i = 0; i < cnf_.getUsers().size(); i++)
				x.push_back(cnf_.getUsers()[i]->name
					+ "   " + boost::lexical_cast<std::string>(cnf_.getUsers()[i]->sessionNumMax)
					+ "   " + boost::lexical_cast<std::string>(cnf_.getUsers()[i]->sessionNumIPMax)
					+ (cnf_.getUsers()[i]->adm ? "   [adm]" : ""));
		}
		std::sort(x.begin(), x.end());
		sendAdm("Total: " + boost::lexical_cast<std::string>(x.size()));
		for (size_t i = 0; i < x.size(); i++)
			sendAdm(x[i]);
	}
	else if (boost::regex_match(command, reLsConn)) {
		std::vector<std::string> x;
		{
			boost::shared_lock<boost::shared_mutex> lock(cnf_.sessionsMutex_);
			for (size_t i = 0; i < cnf_.getSessions().size(); i++)
				if (cnf_.getSessions()[i]->user_)
					x.push_back(cnf_.getSessions()[i]->user_->name + "   "
						+ cnf_.getSessions()[i]->ident_ + "\017@"
						+ cnf_.getSessions()[i]->remoteCC_ + ":"
						+ cnf_.getSessions()[i]->remoteHost_ + "   "
						+ cnf_.getSessions()[i]->network_ + "   "
						+ cnf_.getSessions()[i]->nick_ + "   "
						+ epochToHuman(cnf_.getSessions()[i]->startTime_));
		}
		std::sort(x.begin(), x.end());
		sendAdm("Total: " + boost::lexical_cast<std::string>(x.size()));
		for (size_t i = 0; i < x.size(); i++)
			sendAdm(x[i]);
	}
	else if (boost::regex_match(command, reLsAccess)) {
		boost::shared_lock<boost::shared_mutex> lock(cnf_.accessMutex_);
		sendAdm("Total: " + boost::lexical_cast<std::string>(cnf_.getAccess().size()));
		for (size_t i = 0; i < cnf_.getAccess().size(); i++)
			sendAdm(boost::lexical_cast<std::string>(i) + ": "
				+ (cnf_.getAccess()[i].allow() ? "allow " : "deny  ")
				+ cnf_.getAccess()[i].mask());
	}
	else if (boost::regex_match(command, match, reAccessAdd)) {
		cnf_.accessAdd(match[1] == "allow", match[2]);
		sendAdm("Done.");
	}
	else if (boost::regex_match(command, match, reAccessDel)) {
		cnf_.accessDel(match[1] == "allow", match[2]);
		sendAdm("Done.");
	}
	else if (boost::regex_match(command, match, reUserAdd))
		if (cnf_.userAdd(match[1], match[2]))
			sendAdm("Failed. User already exists... probably.");
		else
			sendAdm("Done.");
	else if (boost::regex_match(command, match, reUserDel))
		if (cnf_.userDel(match[1]))
			sendAdm("Failed.");
		else
			sendAdm("Done.");
	else if (boost::regex_match(command, match, reUserModPw))
		if (cnf_.userMod(match[1], match[2]))
			sendAdm("Failed. No such user... probably.");
		else
			sendAdm("Done.");
	else if (boost::regex_match(command, match, reUserModMax))
		if (cnf_.userMod(match[1], 0, boost::lexical_cast<size_t>(match[2])))
			sendAdm("Failed. No such user... probably.");
		else
			sendAdm("Done.");
	else if (boost::regex_match(command, match, reUserModIP))
		if (cnf_.userMod(match[1], 1, boost::lexical_cast<size_t>(match[2])))
			sendAdm("Failed. No such user... probably.");
		else
			sendAdm("Done.");
	else if (boost::regex_match(command, match, reUserModAdm))
		if (cnf_.userMod(match[1], boost::lexical_cast<bool>(match[2])))
			sendAdm("Failed. No such user... probably.");
		else
			sendAdm("Done.");
	else
		sendAdm("Unrecognized command. Type: 'help' for help.");
}
