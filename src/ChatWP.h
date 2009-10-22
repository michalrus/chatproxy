#ifndef CHATWP_H_e7e6279e639240569ef77fd2ede490e5
#define CHATWP_H_e7e6279e639240569ef77fd2ede490e5

#include "Chat.h"
#include "Http.h"
#include "Tcp.h"

class Session;

class ChatWP : public Chat {
	public:
		ChatWP (Session* session);
		~ChatWP ();
		void init();
		void process(std::vector<std::string>& im);

	private:
		void handle_connect ();
		void handle_read ();
		void handle_end ();

		int logIn ();
		std::string magic (const std::string& smagic);

		Http ht_;

		boost::filesystem::path tokenFile_;
		std::string token_, magic_, ticket_, nickHash_;

		bool tokenNeeded_;

		const boost::regex reCut_, reUser_, reFcuk_;
		bool sent001_, sentBl_;

		// needs to be destructed first, ~Tcp blocks until all events ended
		Tcp tcp_;
};

#endif
