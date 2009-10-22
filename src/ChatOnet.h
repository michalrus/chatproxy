#ifndef CHATONET_H_e7e6279e639240569ef77fd2ede490e5
#define CHATONET_H_e7e6279e639240569ef77fd2ede490e5

#include "Chat.h"

#include <boost/regex.hpp>
#include "Tcp.h"

class Tcp;
class Session;

class ChatOnet : public Chat {
	public:
		ChatOnet (Session* session);
		~ChatOnet ();
		void init();
		void process(std::vector<std::string>& im);

	private:
		void handle_connect ();
		void handle_read ();
		void handle_end ();

		int getUoKey ();
		static std::string authkey (const std::string& key);

		std::string uoKey_;
		const boost::regex reModC_, reModF_, reModI_;
		bool sentBl_;

		std::vector<Channel> list_;

		// needs to be destructed first, ~Tcp blocks until all events ended
		Tcp tcp_;
};

#endif
