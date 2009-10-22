#ifndef SESSION_H_e7e6279e639240569ef77fd2ede490e5
#define SESSION_H_e7e6279e639240569ef77fd2ede490e5

#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>

#include "Tcp.h"

class Config;
class Chat;
struct User;

class Session : public boost::noncopyable {
	public:
		Session (Config& cnf, boost::asio::io_service& io_service);
		~Session ();

		void start ();
		void end (const std::string& reason = "");

		bool deletable () const { return deletable_; }

		static void readIM (std::istream& is, std::vector<std::string>& im);
		static void writeIM (std::ostream& os, const std::vector<std::string>& im, bool lastText = 0);

		void send (const std::vector<std::string>& im, bool lastText = 0);
		void sendInfo (const std::string& msg, const std::string& to = "*");
		void sendAdm (const std::string& msg);
		void sendRaw (const std::string& data);

		static std::string epochToHuman (unsigned int epoch, bool onlyHMS = 0);

		std::string mask () { return ident_ + "@" + remoteCC_ + ":" + remoteHost_; }

		boost::shared_ptr<User> user_;
		friend class Chat;

	private:
		void handle_read ();
		void handle_end ();

		void ident_handle_connect ();
		void ident_handle_end ();
		void ident_handle_read ();

		void auth (std::vector<std::string>& im);
		void adm (const std::string& command);

		boost::asio::io_service& io_service_;
		Config& cnf_;
		unsigned int startTime_;
		bool deletable_;
		std::string myHost_, myPort_, remoteHost_, remotePort_, remoteCC_, ident_, network_,
			nick_, username_, password_, realname_, authUser_, authPass_;
		bool authGotNick_, authGotUser_, authGotPass_;

		boost::shared_ptr<Chat> chat_;

		boost::mutex identMutex_;
		boost::condition_variable identCond_;
		bool identReady_;

	public:
		// needs to be destructed first, ~Tcp blocks until all events ended
		Tcp tcp_, tcpIdent_;
};

#endif
