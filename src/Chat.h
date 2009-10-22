#ifndef CHAT_H_e7e6279e639240569ef77fd2ede490e5
#define CHAT_H_e7e6279e639240569ef77fd2ede490e5

#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/asio/io_service.hpp>
#include <vector>
#include <string>

class Session;

class Chat : public boost::noncopyable {
	public:
		struct Channel {
			std::string name;
			size_t num;
			std::string topic;

			bool operator< (const Channel& rhs) const {
				if (num == rhs.num)
					return name < rhs.name;
				return num < rhs.num;
			}
		};

		Chat (Session* session);

		virtual void init() = 0;
		virtual void process(std::vector<std::string>& im) = 0;

	protected:
		void end (const std::string& reason = "");
		void send (const std::vector<std::string>& im, bool lastText = 0);
		void sendInfo (const std::string& msg, const std::string& to = "*");
		void sendRaw (const std::string& data);

		static std::string epochToHuman (unsigned int epoch, bool onlyHMS = 0);

		boost::tuple<boost::filesystem::path, std::string> genToken ();

		bool adm () const;

		boost::asio::io_service& io_service_;
		std::string& nick_, username_, password_, realname_;

	private:
		Session* session_;
};

#endif
