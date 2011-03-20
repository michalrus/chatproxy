/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Http.h -- libcurl wrapper (asynchronization with asio::io_service)
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

#ifndef HTTP_H_c3dca5908aed43f251ac818e4cf02817
#	define HTTP_H_c3dca5908aed43f251ac818e4cf02817

#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <queue>
#include <map>

class Http : public boost::noncopyable
{
public:
	typedef boost::function<void(bool, boost::shared_ptr<std::string>)> handler_t;

	Http ();
	~Http ();

	/**
	 * Starts loop() thread. You can add request to queue,
	 * but nothing will happen if loop() is started.
	 */
	static void init (boost::asio::io_service& io_service, const std::string& vhost = "*");

	void async_get (const std::string& url, handler_t handler);
	void async_post (const std::string& url, const std::string post_data, handler_t handler);

	static std::string urlencode (const std::string& c);

	inline static const std::string& vhost () { return vhost_; }
	inline static boost::asio::io_service& io_service () { return *io_service_; }

private:
	/**
	 * Cookies (remembered per instance).
	 */
	boost::shared_ptr<std::map<std::string, std::string> > cookies_;

	/**
	 * Getter/poster loop. Run in a separate thread
	 * to "asynchronize" libcurl.
	 */
	static void loop (boost::asio::io_service& io_service);

	static size_t write_func (char* data, size_t size, size_t nmemb, std::string* buf);
	static size_t headr_func (char* data, size_t size, size_t nmemb, std::map<std::string, std::string>* cookies);

	struct Request
	{
		Request() : get(true) {}

		std::string url;
		bool get;
		std::string post_data;
		handler_t handler;
		boost::shared_ptr<std::map<std::string, std::string> > cookies;
	};

	static std::queue<Request> queue_;
	static boost::mutex queue_mutex_;
	static boost::condition_variable queue_cond_;

	static std::string vhost_;
	static boost::asio::io_service* io_service_;
	static bool inited_;
	static void* curl_;
	static char err_buf_[128];
};

#endif
