#ifndef CONFIG_H_e7e6279e639240569ef77fd2ede490e5
#define CONFIG_H_e7e6279e639240569ef77fd2ede490e5

#include <boost/noncopyable.hpp>
#include <boost/regex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <vector>
#include <cryptopp/osrng.h>

class Session;

struct User : public boost::noncopyable {
	std::string name;
	std::string password;
	size_t sessionNumMax;
	size_t sessionNumIPMax;
	bool adm;
	bool locked;
};

struct Access {
	Access (bool allow, const std::string& mask)
		: allow_(allow), mask_(mask)
	{
		std::string tmp = mask_;
		boost::regex x("([\\\\.\\[\\]|(){}<>\\^\\$\\-+])"), y("\\*"), z("\\?");
		tmp = boost::regex_replace(tmp, x, "\\\\${1}");
		tmp = boost::regex_replace(tmp, y, ".*");
		tmp = boost::regex_replace(tmp, z, ".");
		reg_ = boost::regex(tmp, boost::regex_constants::icase);
	}

	bool match (const std::string& host) const
	{
		return boost::regex_match(host, reg_);
	}

	bool allow () const
	{
		return allow_;
	}

	const std::string& mask () const
	{
		return mask_;
	}

	bool operator< (const Access& rhs) const
	{
		if (match(rhs.mask_))
			return 1;
		if (allow_ == rhs.allow_)
			return mask_ < rhs.mask_;
		return allow_;
	}

private:
	bool allow_;
	std::string mask_;
	boost::regex reg_;
};

class Config : public boost::noncopyable {
	public:
		Config (const boost::filesystem::path& path);
		boost::shared_ptr<User> authenticate (const std::string& user, const std::string& password);
		std::string upassword (const std::string& user);

		boost::filesystem::ofstream& errorLog ();
		size_t getThreads () const;
		unsigned short getPort () const;

		boost::tuple<boost::filesystem::path, std::string> genToken ();

		const std::vector<boost::shared_ptr<User> >& getUsers () const { return users_; }
		int userAdd (const std::string& name, const std::string& password);
		int userDel (const std::string& name);
		int userMod (const std::string& name, const std::string& password);
		int userMod (const std::string& name, bool ip, size_t limit);
		int userMod (const std::string& name, bool adm);
		void usersCleanup ();
		void usersSave ();

		const std::vector<Access>& getAccess () const { return access_; }
		void accessAdd (bool allow, const std::string& mask);
		void accessDel (bool allow, const std::string& mask);
		void accessSave ();

		const std::vector<boost::shared_ptr<Session> >& getSessions() const { return sessions_; }
		void sessionAdd (boost::shared_ptr<Session>& s);
		void sessionsCleanup ();

		boost::shared_mutex usersMutex_;
		boost::shared_mutex accessMutex_;
		boost::shared_mutex sessionsMutex_;

	private:
		void readConfig ();
		void readUsers ();
		void readAccess ();

		CryptoPP::AutoSeededRandomPool rng_;

		boost::filesystem::path fileConfig_;
		boost::filesystem::path fileAccess_;
		boost::filesystem::path fileUsers_;
		boost::filesystem::path fileError_;
		boost::filesystem::path dirToken_;
		std::string locToken_;
		boost::filesystem::ofstream osError_;

		std::vector<boost::shared_ptr<User> > users_;
		std::vector<Access> access_;
		std::vector<boost::shared_ptr<Session> > sessions_;

		size_t threads_;
		unsigned short port_;
		const boost::regex reCmt_;
};

#endif
