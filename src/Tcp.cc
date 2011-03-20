/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Tcp.cc -- asynchronous TCP wrapper (shutdowns cleanly anytime).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Tcp.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "Debug.h"

Tcp::Tcp (boost::asio::io_service& io_service,
	boost::function<void()> handle_read,
	boost::function<void(const std::string&)> handle_end,
	boost::function<void()> handle_connect)
	: impl_(new impl(io_service, handle_read, handle_end, handle_connect))
{
}

Tcp::impl::impl (boost::asio::io_service& io_service,
	boost::function<void()> handle_read,
	boost::function<void(const std::string&)> handle_end,
	boost::function<void()> handle_connect)
	: socket_(io_service), io_service_(io_service), timer_(io_service), timeout_(0),
	  handle_read_(handle_read), handle_end_(handle_end), handle_connect_(handle_connect),
	  vhost_(false), ending_(false), connecting_(false)
{
}

Tcp::~Tcp ()
{
	/* if "visible" object gets destroyed, we don't want
	 * to call Tcp::handle_end_ -- it probably was destroyed
	 * too -- furthermore, owner of Tcp *knows* that connection
	 * will end in microseconds -- all in all he "called" ~Tcp. =)
	 */
	impl_->handle_end_ = boost::function<void(const std::string&)>();

	impl_->end("");
}

Tcp::impl::~impl ()
{
}

void Tcp::timeout (unsigned int tmout)
{
	impl_->timeout(tmout, impl_);
}

void Tcp::impl::timeout (unsigned int tmout, boost::shared_ptr<Tcp::impl> me)
{
	if (ending_)
		return;

	timeout_ = tmout;
	if (!timeout_) {
		timer_.cancel();
		return;
	}
	timer_.expires_from_now(boost::posix_time::seconds(timeout_));
	timer_.async_wait(boost::bind(&Tcp::impl::handle_timeout, this, boost::asio::placeholders::error, me));
}

void Tcp::vhost (const std::string& addr)
{
	impl_->vhost(addr);
}

void Tcp::impl::vhost (const std::string& addr)
{
	if (vhost_ || connecting_ || ending_ || addr == "*")
		return;
	vhost_ = true;

	try {
		boost::asio::ip::tcp::endpoint endpoint;
		endpoint.address(boost::asio::ip::address_v4::from_string(addr));

		socket_.open(boost::asio::ip::tcp::v4());
		socket_.bind(endpoint);
	}
	catch (...) {
		die("vhost: cannot assign specified address");
	}
}

void Tcp::connect (const std::string& host, unsigned short port)
{
	impl_->connect(host, port, impl_);
}

void Tcp::impl::connect (const std::string& host, unsigned short port, boost::shared_ptr<Tcp::impl> me)
{
	if (connecting_ || ending_)
		return;

	connecting_ = true;

	boost::asio::ip::tcp::resolver resolver(io_service_);
	endpoint_iterator_ = resolver.resolve(boost::asio::ip::tcp::resolver::query(host,
		boost::lexical_cast<std::string>(port)));

	boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator_;
	++endpoint_iterator_;
	socket_.async_connect(endpoint, boost::bind(&Tcp::impl::handle_connect, this,
		boost::asio::placeholders::error, me));
}

void Tcp::end (const std::string& reason)
{
	impl_->end(reason);
}

void Tcp::impl::end (const std::string& reason)
{
	if (ending_)
		return;
	ending_ = true;

	if (socket_.is_open()) {
		/* flush buffers... */
		try {
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
		}
		catch (...) { /* bad file descriptor */ }

		/* ... and close...*/
		socket_.close();
	}

	/* ... and cancel timeout timer; */
	timer_.cancel();

	/* after above calls all "waiting" handlers will eventually
	 * get called, destroying all "shared_from_this()" smart
	 * pointers and thus calling Tcp::impl::~impl if ~Tcp was
	 * called -- or calling impl::handle_end_ and waiting for ~Tcp.
	 */

	/* Post end handler in Tcp owner -- if connection was shutdown
	 * explicitly, by Tcp::end(), handler should cause owner deletion
	 * (or at least Tcp object deletion);
	 * if implicitly (by Tcp::~Tcp()), then (bool )handle_end_ == false.
	 */
	if (handle_end_)
		io_service_.post(boost::bind(handle_end_, reason));
}

void Tcp::read_until (const std::string& delim)
{
	impl_->read_until(delim, impl_);
}

void Tcp::impl::read_until (const std::string& delim, boost::shared_ptr<Tcp::impl> me)
{
	if (ending_)
		return;

	try {
		boost::asio::async_read_until(socket_, buffer_, delim,
			boost::bind(&Tcp::impl::handle_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				me));
	}
	catch (...) {
		end("read_until: bad file descriptor");
	}
}

void Tcp::write (const std::string& data)
{
	impl_->write(data, impl_);
}

void Tcp::impl::write (const std::string& data, boost::shared_ptr<Tcp::impl> me)
{
	if (ending_)
		return;

	boost::shared_ptr<std::string> tmp(new std::string(data));

	try {
		boost::asio::async_write(socket_, boost::asio::buffer(*tmp), boost::bind(&Tcp::impl::handle_write,
			this, tmp, boost::asio::placeholders::error, me));
	}
	catch (...) {
		end("write: bad file descriptor");
	}
}

std::string Tcp::data () const
{
	return impl_->data_;
}

void Tcp::impl::handle_timeout (const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me)
{
	if (error != boost::asio::error::operation_aborted)
		end("timeout");
}

void Tcp::impl::handle_read (const boost::system::error_code& error, size_t bytes_transferred, boost::shared_ptr<Tcp::impl> me)
{
	if (error)
		end("handle_read: " + error.message());
	else if (!ending_) {
		timeout(timeout_, me);

		{
			char data[bytes_transferred];
			std::istream(&buffer_).read(data, bytes_transferred);
			data_ = std::string(data, bytes_transferred);
		}

		if (handle_read_)
			handle_read_();
	}
}

void Tcp::impl::handle_write (boost::shared_ptr<std::string> tmp, const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me)
{
	if (error)
		end("handle_write: " + error.message());
}

void Tcp::impl::handle_connect (const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me)
{
	if (!error) {
		if (!ending_) {
			timeout(timeout_, me);
			if (handle_connect_)
				handle_connect_();
		}
	}
	else if (endpoint_iterator_ != boost::asio::ip::tcp::resolver::iterator()) {
		socket_.close();
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator_;
		++endpoint_iterator_;
		socket_.async_connect(endpoint, boost::bind(&Tcp::impl::handle_connect, this,
			boost::asio::placeholders::error, me));
	}
	else
		end("handle_connect: " + error.message() + " (check vhost)");
}
