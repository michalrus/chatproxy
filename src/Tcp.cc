#include "Tcp.h"

#include <istream>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

Tcp::Tcp (boost::asio::io_service& io_service,
	boost::function<void()> funcRead, boost::function<void()> funcEnd, boost::function<void()> funcConnect, const std::string& debug)
	: socket_(io_service), timer_(io_service), timeout_(0), io_service_(io_service), funcRead_(funcRead),
	  funcEnd_(funcEnd), funcConnect_(funcConnect), connecting_(0), ending_(0), closed_(0), nev_(0),
	  debug_(debug)
{
}

Tcp::~Tcp ()
{
	close();

	// and wait for do_end to... end (nev_ == 0)

	{
		boost::unique_lock<boost::mutex> lock(nevMutexE_);
		while (nev_)
			nevCond_.wait(lock);
	}
}

void Tcp::timeout (size_t tmout)
{
	nevIncrm();
	io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_timeout, this, tmout))));
}

void Tcp::connect (const std::string& host, unsigned short port)
{
	if (connecting_)
		return;
	connecting_ = 1;
	boost::unique_lock<boost::shared_mutex> lock(socketMutex_);
	boost::asio::ip::tcp::resolver resolver(io_service_);
	endpoint_iterator_ = resolver.resolve(boost::asio::ip::tcp::resolver::query(host,
		boost::lexical_cast<std::string>(port)));

	boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator_;
	++endpoint_iterator_;
	nevIncrm();
	socket_.async_connect(endpoint, boost::bind(&Tcp::handle_connect, this,
		boost::asio::placeholders::error));
}

void Tcp::close ()
{
	nevIncrm();
	io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_close, this))));
}

void Tcp::readUntil (const std::string& delim)
{
	nevIncrm();
	io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_readUntil, this, delim, 1))));
}

void Tcp::write (const std::string& data)
{
	boost::unique_lock<boost::shared_mutex> lock(queueWMutex_);
	boost::shared_ptr<std::string> tmp(new std::string(data));
	queueW_.push(tmp);
	nevIncrm();
	io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_write, this))));
}

int Tcp::get (std::string& buffer)
{
	boost::unique_lock<boost::shared_mutex> lock(queueRMutex_);
	if (queueR_.empty())
		return -1;
	buffer = *(queueR_.front());
	queueR_.pop();
	return 0;
}

void Tcp::nevIncrm ()
{
	boost::unique_lock<boost::mutex> lock(nevMutex_);
	nev_++;
}

void Tcp::nevDecrm ()
{
	boost::unique_lock<boost::mutex> lock(nevMutex_);
	nev_--;

	if (nev_ < 2)
		nevCond_.notify_all();
}

void Tcp::nevWrap (boost::function<void()> func)
{
	func();
	nevDecrm();
}

void Tcp::handle_timeout (const boost::system::error_code& error)
{
	if (error != boost::asio::error::operation_aborted)
		close();
	nevDecrm();
}

void Tcp::handle_connect (const boost::system::error_code& error)
{
	boost::unique_lock<boost::shared_mutex> lock(socketMutex_);
	error_ = error;

	if (!error) {
		timeout(timeout_);
		connecting_ = 0;
		if (funcConnect_)
			funcConnect_();
	}
	else if (endpoint_iterator_ != boost::asio::ip::tcp::resolver::iterator()) {
		socket_.close();
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator_;
		++endpoint_iterator_;
		nevIncrm();
		socket_.async_connect(endpoint, boost::bind(&Tcp::handle_connect, this,
			boost::asio::placeholders::error));
	}
	else {
		nevIncrm();
		io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_end, this))));
	}

	nevDecrm();
}

void Tcp::handle_read (boost::shared_ptr<boost::unique_lock<boost::shared_mutex> > bufLock,
	const boost::system::error_code& error, size_t bytes_transferred)
{
	error_ = error;

	if (error) {
		nevIncrm();
		io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_end, this))));
	}
	else {
		timeout(timeout_);
		boost::unique_lock<boost::shared_mutex> lock(queueRMutex_);
		char data[bytes_transferred];
		std::istream(&buffer_).read(data, bytes_transferred);
		boost::shared_ptr<std::string> tmp(new std::string(data, bytes_transferred));
		queueR_.push(tmp);
		if (funcRead_) {
			nevIncrm();
			io_service_.post(boost::bind(&Tcp::nevWrap, this, funcRead_));
		}
	}

	nevDecrm();
}

void Tcp::handle_write (boost::shared_ptr<std::string> tmp, const boost::system::error_code& error)
{
	error_ = error;

	if (error) {
		nevIncrm();
		io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_end, this))));
	}
//	else if (!queueW_.empty()) {
//		nevIncrm();
//		io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_write, this))));
//	}

	nevDecrm();
}

void Tcp::do_timeout (size_t tmout)
{
	timeout_ = tmout;
	boost::unique_lock<boost::shared_mutex> lock(timerMutex_);
	if (!timeout_) {
		timer_.cancel();
		return;
	}
	timer_.expires_from_now(boost::posix_time::seconds(timeout_));
	nevIncrm();
	timer_.async_wait(boost::bind(&Tcp::handle_timeout, this, boost::asio::placeholders::error));
}

void Tcp::do_close ()
{
	if (closed_)
		return;
	closed_ = 1;

	boost::unique_lock<boost::shared_mutex> lock(socketMutex_);
	if (socket_.is_open()) {
		// make sure we'll get read err.
		// funcRead_ = boost::function<void()>();
		// do_readUntil("\n", 0);

		socket_.cancel();
		socket_.close();
	}
	else {
		nevIncrm();
		io_service_.post(boost::bind(&Tcp::nevWrap, this, boost::protect(boost::bind(&Tcp::do_end, this))));
	}
}

void Tcp::do_readUntil (std::string delim, bool do_lock)
{
	if (do_lock)
		boost::unique_lock<boost::shared_mutex> lock(socketMutex_);
	boost::shared_ptr<boost::unique_lock<boost::shared_mutex> > bufLock(new boost::unique_lock<boost::shared_mutex>(bufferMutex_));
	nevIncrm();
	boost::asio::async_read_until(socket_, buffer_, delim, boost::bind(&Tcp::handle_read,
		this, bufLock, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void Tcp::do_write ()
{
	boost::unique_lock<boost::shared_mutex> lock(queueWMutex_);
	boost::unique_lock<boost::shared_mutex> lock2(socketMutex_);
	if (queueW_.empty())
		return;
	boost::shared_ptr<std::string> tmp = queueW_.front();
	nevIncrm();
	boost::asio::async_write(socket_, boost::asio::buffer(*tmp), boost::bind(&Tcp::handle_write,
		this, tmp, boost::asio::placeholders::error));
	queueW_.pop();
}

void Tcp::do_end ()
{
	closed_ = 1;
	if (ending_)
		return;
	ending_ = 1;
	timer_.cancel();

	// wait for all operations to complete (nev_ == 1 - for do_end() itself),
	// so that *this can be safely deleted in funcEnd_

	{
		boost::unique_lock<boost::mutex> lock(nevMutexE_);
		while (nev_ > 1)
			nevCond_.wait(lock);
	}

//	std::cout << debug_ << ".2" << std::endl;

	boost::this_thread::sleep(boost::posix_time::milliseconds(100));

//	std::cout << debug_ << ".3" << std::endl;

	if (funcEnd_)
		funcEnd_();
}
