/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Tcp.h -- asynchronous TCP wrapper (shutdowns cleanly anytime).
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

#ifndef TCP_H_c3dca5908aed43f251ac818e4cf02817
#	define TCP_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>

class Tcp : public boost::noncopyable
{
public:
	Tcp (boost::asio::io_service& io_service,
		boost::function<void()> handle_read,
		boost::function<void(const std::string&)> handle_end,
		boost::function<void()> handle_connect);

	/**
	 * Implicitly shutdowns connecion.
	 */
	~Tcp ();

	/**
	 * Sets read @tmout.
	 */
	void timeout (unsigned int tmout);

	/**
	 * Makes socket_ use given vhost to .connect()
	 */
	void vhost (const std::string& addr);

	/**
	 * Connects to remote @host:@port.
	 */
	void connect (const std::string& host, unsigned short port);

	/**
	 * Explicitly shutdowns connecion.
	 */
	void end (const std::string& reason = "");

	/**
	 * Reads data from @socket_ until @delim occurs; stores it
	 * in @buf_, then calls @handle_read_.
	 */
	void read_until (const std::string& delim);

	/**
	 * Writes data to @socket_.
	 */
	void write (const std::string& data);

	/**
	 * Gets @buf_.
	 */
	std::string data () const;

	boost::asio::ip::tcp::socket& socket ()
	{
		return impl_->socket_;
	}

private:
	class impl
	{
	public:
		impl (boost::asio::io_service& io_service,
			boost::function<void()> handle_read,
			boost::function<void(const std::string&)> handle_end,
			boost::function<void()> handle_connect);
		~impl ();

		void timeout (unsigned int tmout, boost::shared_ptr<Tcp::impl> me);
		void vhost (const std::string& addr);
		void connect (const std::string& host, unsigned short port, boost::shared_ptr<Tcp::impl> me);
		void end (const std::string& reason);
		void read_until (const std::string& delim, boost::shared_ptr<Tcp::impl> me);
		void write (const std::string& data, boost::shared_ptr<Tcp::impl> me);

		void handle_timeout (const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me);
		void handle_read (const boost::system::error_code& error, size_t bytes_transferred, boost::shared_ptr<Tcp::impl> me);
		void handle_write (boost::shared_ptr<std::string> tmp, const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me);
		void handle_connect (const boost::system::error_code& error, boost::shared_ptr<Tcp::impl> me);

		boost::asio::ip::tcp::socket socket_;
		boost::asio::io_service& io_service_;
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator_;
		boost::asio::deadline_timer timer_;
		size_t timeout_;

		boost::asio::streambuf buffer_;
		std::string data_;

		boost::function<void()> handle_read_;
		boost::function<void(const std::string&)> handle_end_;
		boost::function<void()> handle_connect_;
		bool vhost_, ending_, connecting_;
	};

	boost::shared_ptr<impl> impl_;
};

#endif
