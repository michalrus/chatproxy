#ifndef TCP_H_e7e6279e639240569ef77fd2ede490e5
#define TCP_H_e7e6279e639240569ef77fd2ede490e5

#include <string>
#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

class Tcp : public boost::noncopyable
{
	public:
		Tcp (boost::asio::io_service& io_service,
			boost::function<void()> funcRead, boost::function<void()> funcEnd, boost::function<void()> funcConnect, const std::string& debug = "");
		~Tcp ();

		void timeout (size_t tmout);
		void connect (const std::string& host, unsigned short port);
		void close ();
		void readUntil (const std::string& delim);
		void write (const std::string& data);
		int get (std::string& buffer);

		boost::asio::ip::tcp::socket socket_;
		boost::system::error_code error_;

	private:
		void nevIncrm ();
		void nevDecrm ();
		void nevWrap (boost::function<void()> func);

		void handle_timeout (const boost::system::error_code& error);
		void handle_connect (const boost::system::error_code& error);
		void handle_read (boost::shared_ptr<boost::unique_lock<boost::shared_mutex> > bufLock,
			const boost::system::error_code& error, size_t bytes_transferred);
		void handle_write (boost::shared_ptr<std::string> tmp, const boost::system::error_code& error);

		void do_timeout (size_t tmout);
		void do_close ();
		void do_readUntil (std::string delim, bool do_lock);
		void do_write ();
		void do_end ();

		boost::asio::deadline_timer timer_;
		size_t timeout_;
		boost::asio::streambuf buffer_;
		boost::asio::io_service& io_service_;
		boost::function<void()> funcRead_, funcEnd_, funcConnect_;

		boost::asio::ip::tcp::resolver::iterator endpoint_iterator_;

		boost::shared_mutex socketMutex_, queueWMutex_, queueRMutex_, bufferMutex_, timerMutex_;
		std::queue<boost::shared_ptr<std::string> > queueW_, queueR_;
		bool connecting_, ending_, closed_;

		boost::mutex nevMutex_, nevMutexE_;
		boost::condition_variable nevCond_;
		size_t nev_;
		std::string debug_;
};

#endif
