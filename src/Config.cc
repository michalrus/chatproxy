#include "Config.h"

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>
#include <cryptopp/hex.h>
#include <algorithm>

#include "EException.h"
#include "Session.h"

Config::Config (const boost::filesystem::path& path)
	: fileConfig_(path), threads_(0), port_(0), reCmt_("^\\s*(#.*)?$")
{
	readConfig();
	readAccess();
	readUsers();
}

boost::shared_ptr<User> Config::authenticate (const std::string& user, const std::string& password)
{
	boost::shared_lock<boost::shared_mutex> lock(usersMutex_);
	size_t i;
	bool ok = 0;

	for (i = 0; i < users_.size() && !ok; i++)
		if (!users_[i]->locked && user == users_[i]->name && password == users_[i]->password)
			ok = 1;

	if (ok)
		return users_[i - 1];
	return boost::shared_ptr<User>();
}

std::string Config::upassword (const std::string& user)
{
	boost::shared_lock<boost::shared_mutex> lock(usersMutex_);

	for (size_t i = 0; i < users_.size(); i++)
		if (!users_[i]->locked && user == users_[i]->name)
			return users_[i]->password;

	return "";
}

boost::filesystem::ofstream& Config::errorLog ()
{
	return osError_;
}

size_t Config::getThreads () const
{
	return threads_;
}

unsigned short Config::getPort () const
{
	return port_;
}

boost::tuple<boost::filesystem::path, std::string> Config::genToken ()
{
	byte rnd[3];
	std::string name;

	do {
		rng_.GenerateBlock(rnd, sizeof(rnd));
		CryptoPP::HexEncoder encoder;
		encoder.Attach(new CryptoPP::StringSink(name));
		encoder.Put(rnd, sizeof(rnd));
		encoder.MessageEnd();
		boost::to_lower(name);
	} while (boost::filesystem::is_regular_file(boost::filesystem::status(dirToken_ / name)));

	return boost::make_tuple(dirToken_ / name, locToken_ + name);
}

int Config::userAdd (const std::string& name, const std::string& password)
{
	boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

	for (size_t i = 0; i < users_.size(); i++)
		if (users_[i]->name == name)
			return 1;

	boost::shared_ptr<User> tmp(new User);
	tmp->name = name;
	tmp->password = password;
	tmp->sessionNumMax = 1;
	tmp->sessionNumIPMax = 1;
	tmp->locked = 0;
	tmp->adm = 0;

	users_.push_back(tmp);

	usersSave();
	return 0;
}

int Config::userDel (const std::string& name)
{
	bool doCleanup = 0;

	{
		boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

		bool ok = 0;
		size_t i, j;

		for (i = 0; i < users_.size() && !ok; i++)
			ok = (users_[i]->name == name);
		--i;

		if (!ok)
			return 1;

		users_[i]->locked = 1;

		boost::unique_lock<boost::shared_mutex> lock2(sessionsMutex_);

		j = 0;
		for (i = 0; i < sessions_.size(); i++)
			if (sessions_[i]->user_->name == name) {
				sessions_[i]->end("Session implicitly closed (user deleted).");
				j++;
			}

		if (!j)
			doCleanup = 1;
		// else: will be called by sessionsCleanup()

		usersSave();
	}

	if (doCleanup)
		usersCleanup();

	return 0;
}

int Config::userMod (const std::string& name, const std::string& password)
{
	boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

	bool ok = 0;
	size_t i;
	for (i = 0; i < users_.size() && !ok; i++)
		ok = (users_[i]->name == name);
	--i;

	if (!ok)
		return 1;

	users_[i]->password = password;

	usersSave();
	return 0;
}

int Config::userMod (const std::string& name, bool ip, size_t limit)
{
	boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

	bool ok = 0;
	size_t i;
	for (i = 0; i < users_.size() && !ok; i++)
		ok = (users_[i]->name == name);
	--i;

	if (!ok)
		return 1;

	if (ip)
		users_[i]->sessionNumIPMax = limit;
	else
		users_[i]->sessionNumMax = limit;

	usersSave();
	return 0;
}

int Config::userMod (const std::string& name, bool adm)
{
	boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

	bool ok = 0;
	size_t i;
	for (i = 0; i < users_.size() && !ok; i++)
		ok = (users_[i]->name == name);
	--i;

	if (!ok)
		return 1;

	users_[i]->adm = adm;

	usersSave();
	return 0;
}

void Config::usersCleanup ()
{
	boost::unique_lock<boost::shared_mutex> lock(usersMutex_);

	bool done = 0;

	while (!done)
		for (size_t i = 0; i < users_.size(); i++)
			if (users_[i]->locked) {
				size_t k = 0;
				for (size_t j = 0; j < sessions_.size(); j++)
					if (sessions_[j]->user_ == users_[i]) {
						k++;
						break;
					}

				if (!k)
					users_.erase(users_.begin() + i);
				break;
			}
			else if (i == users_.size() - 1)
				done = 1;
}

void Config::usersSave ()
{
	boost::filesystem::ofstream fs(fileUsers_);

	for (size_t i = 0; i < users_.size(); i++)
		if (!users_[i]->locked)
			fs << users_[i]->name << "    " << users_[i]->password
				<< "    " << users_[i]->sessionNumMax
				<< "    " << users_[i]->sessionNumIPMax << "    "
				<< (users_[i]->adm ? "1" : "0") << std::endl;
}

void Config::accessAdd (bool allow, const std::string& mask)
{
	boost::unique_lock<boost::shared_mutex> lock(accessMutex_);

	Access tmp (allow, mask);
	access_.push_back(tmp);

	accessSave();
}

void Config::accessDel (bool allow, const std::string& mask)
{
	boost::unique_lock<boost::shared_mutex> lock(accessMutex_);
	bool done = 0;

	while (!done)
		if (!access_.size())
			done = 1;
		else for (size_t i = 0; i < access_.size(); i++)
			if (access_[i].allow() == allow && access_[i].mask() == mask) {
				access_.erase(access_.begin() + i);
				break;
			}
			else if (i == access_.size() - 1)
				done = 1;

	accessSave();
}

void Config::accessSave ()
{
	boost::filesystem::ofstream fs(fileAccess_);

	std::sort(access_.begin(), access_.end());
	for (size_t i = 0; i < access_.size(); i++)
		fs << (access_[i].allow() ? "allow " : "deny  ") << access_[i].mask() << std::endl;

	boost::unique_lock<boost::shared_mutex> lock(sessionsMutex_);
	std::vector<boost::shared_ptr<Session> > toEnd;

	for (size_t i = 0; i < getSessions().size(); i++) {
		bool accessOK = 1;
		for (size_t j = 0; j < getAccess().size(); j++)
			if (getAccess()[getAccess().size() - 1 - j].match(getSessions()[i]->mask())) {
				accessOK = getAccess()[getAccess().size() - 1 - j].allow();
				break;
			}

		if (getSessions()[i]->user_ && getSessions()[i]->user_->adm)
			continue;
		if (!accessOK)
			toEnd.push_back(getSessions()[i]);
	}

	for (size_t i = 0; i < toEnd.size(); i++)
		toEnd[i]->end("Banned!");
}

void Config::sessionAdd (boost::shared_ptr<Session>& s)
{
	boost::unique_lock<boost::shared_mutex> lock(sessionsMutex_);

	sessions_.push_back(s);
}

void Config::sessionsCleanup ()
{
	boost::unique_lock<boost::shared_mutex> lock(sessionsMutex_);
	bool done = 0;

	while (!done && sessions_.size())
		for (size_t i = 0; i < sessions_.size(); i++) {
			if (sessions_[i]->deletable()) {
				sessions_.erase(sessions_.begin() + i);
				break;
			}
			if (i == sessions_.size() - 1)
				done = 1;
		}

	usersCleanup();
}

void Config::readConfig ()
{
	if (!boost::filesystem::is_regular_file(boost::filesystem::status(fileConfig_)))
		throw EFail(fileConfig_.string() + ": not a file.");

	boost::filesystem::ifstream file(fileConfig_);
	boost::smatch match;
	std::string line;
	size_t nl = 0;

	port_ = 0;
	fileUsers_ = "";

	boost::regex reUsers("^\\s*Users\\s+(.+)\\s*$");
	boost::regex reAccess("^\\s*Access\\s+(.+)\\s*$");
	boost::regex reError("^\\s*ErrLog\\s+(.+)\\s*$");
	boost::regex rePort("^\\s*Port\\s+([1-9][0-9]*)\\s*$");
	boost::regex reThreads("^\\s*Threads\\s+([1-9][0-9]*)\\s*$");
	boost::regex reTokenDir("^\\s*TokenDir\\s+(.+)\\s*$");
	boost::regex reTokenLoc("^\\s*TokenLoc\\s+(.+)\\s*$");

	while (!file.eof()) {
		std::getline(file, line); nl++;

		if (boost::regex_match(line, reCmt_))
			continue;

		if (boost::regex_match(line, match, reUsers))
			fileUsers_ = fileConfig_.parent_path() / std::string(match[1]);
		else if (boost::regex_match(line, match, reAccess))
			fileAccess_ = fileConfig_.parent_path() / std::string(match[1]);
		else if (boost::regex_match(line, match, reError)) {
			fileError_ = fileConfig_.parent_path() / std::string(match[1]);
			if (!boost::filesystem::is_regular_file(boost::filesystem::status(fileError_)))
				throw EFail(fileError_.string() + ": not a file.");
			osError_.open(fileError_);
		}
		else if (boost::regex_match(line, match, rePort))
			port_ = boost::lexical_cast<unsigned short>(match[1]);
		else if (boost::regex_match(line, match, reThreads))
			threads_ = boost::lexical_cast<size_t>(match[1]);
		else if (boost::regex_match(line, match, reTokenDir)) {
			dirToken_ = fileConfig_.parent_path() / std::string(match[1]);
			if (!boost::filesystem::is_directory(boost::filesystem::status(dirToken_)))
				throw EFail(dirToken_.string() + ": not a directory.");
		}
		else if (boost::regex_match(line, match, reTokenLoc))
			locToken_ = match[1];
		else
			throw EFail(fileConfig_.string() + "[" + boost::lexical_cast<std::string>(nl) + "]: syntax error.");
	}

	if (!port_ || fileUsers_.empty() || fileAccess_.empty() || fileError_.empty() || !threads_ || dirToken_.empty() || locToken_.empty())
		throw EFail(fileConfig_.string() + ": you must specify Port and Users and Access and ErrLog and Thread *and* TokenDir&Loc!");
}

void Config::readUsers ()
{
	if (!boost::filesystem::is_regular_file(boost::filesystem::status(fileUsers_)))
		throw EFail(fileUsers_.string() + ": not a file.");

	boost::filesystem::ifstream file(fileUsers_);
	boost::smatch match;
	std::string line;
	size_t nl = 0;
	User tmp;

	boost::regex reUser("^\\s*([^\\s]+)\\s+([^\\s]+)\\s+([1-9][0-9]*)\\s+([1-9][0-9]*)\\s+([01])\\s*$");

	while (!file.eof()) {
		std::getline(file, line); nl++;

		if (boost::regex_match(line, reCmt_))
			continue;
		if (boost::regex_match(line, match, reUser)) {
			boost::shared_ptr<User> tmp(new User);

			tmp->name = match[1];
			tmp->password = match[2];
			tmp->sessionNumMax = boost::lexical_cast<size_t>(match[3]);
			tmp->sessionNumIPMax = boost::lexical_cast<size_t>(match[4]);
			tmp->adm = boost::lexical_cast<bool>(match[5]);
			tmp->locked = 0;

			users_.push_back(tmp);
		}
		else
			throw EFail(fileUsers_.string() + "[" + boost::lexical_cast<std::string>(nl) + "]: syntax error.");
	}

	file.close();
}

void Config::readAccess ()
{
	if (!boost::filesystem::is_regular_file(boost::filesystem::status(fileAccess_)))
		throw EFail(fileAccess_.string() + ": not a file.");

	boost::filesystem::ifstream file(fileAccess_);
	boost::smatch match;
	std::string line;
	size_t nl = 0;

	boost::regex reAccess("^\\s*(allow|deny)\\s+([^\\s]+)\\s*$");

	while (!file.eof()) {
		std::getline(file, line); nl++;

		if (boost::regex_match(line, reCmt_))
			continue;
		if (boost::regex_match(line, match, reAccess)) {
			Access tmp(match[1] == "allow", match[2]);
			access_.push_back(tmp);
		}
		else
			throw EFail(fileAccess_.string() + "[" + boost::lexical_cast<std::string>(nl) + "]: syntax error.");
	}

	file.close();
	std::sort(access_.begin(), access_.end());
}
