/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * Http.cc -- libcurl wrapper (asynchronization with asio::io_service)
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

#include "Http.h"

#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <curl/curl.h>

#include "Debug.h"

Http::Http ()
	: cookies_(new std::map<std::string, std::string>)
{
}

Http::~Http ()
{
}

int Http__sockt_func (boost::asio::ip::tcp::socket* socket, curl_socket_t curlfd, curlsocktype purpose);

void Http::init (boost::asio::io_service& io_service, const std::string& vhost)
{
	if (inited_)
		return;

	vhost_ = vhost;
	io_service_ = &io_service;

	curl_ = curl_easy_init();

	std::memset(err_buf_, 0, sizeof(err_buf_));

	curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, err_buf_);
	curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.1.3) Gecko/20090824 Firefox/3.5.3");
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, headr_func);
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl_, CURLOPT_SOCKOPTFUNCTION, Http__sockt_func);

	boost::thread(boost::bind(&Http::loop, boost::ref(io_service)));
}

void Http::async_get (const std::string& url, handler_t handler)
{
	Request tmp;
	tmp.url = url;
	tmp.handler = handler;
	tmp.cookies = cookies_;
	{
		boost::lock_guard<boost::mutex> lock(queue_mutex_);
		queue_.push(tmp);
	}
	queue_cond_.notify_all();
}

void Http::async_post (const std::string& url, const std::string post_data,	handler_t handler)
{
	Request tmp;
	tmp.url = url;
	tmp.get = false;
	tmp.post_data = post_data;
	tmp.handler = handler;
	tmp.cookies = cookies_;
	{
		boost::lock_guard<boost::mutex> lock(queue_mutex_);
		queue_.push(tmp);
	}
	queue_cond_.notify_all();
}

std::string Http::urlencode (const std::string& c)
{
	std::string r;

	for (size_t i = 0; i < c.size(); i++)
		if (('0' <= c[i] && c[i] <= '9')
			|| ('A' <= c[i] && c[i] <= 'Z')
			|| ('a' <= c[i] && c[i] <= 'z')
			|| c[i] == '_' || c[i] == '-' || c[i] == '.')
			r += c[i];
		else {
			char dig1 = (c[i] & 0xF0) >> 4;
			char dig2 = (c[i] & 0x0F);

			if (0 <= dig1 && dig1 <= 9)
				dig1 += 48;
			if (10 <= dig1 && dig1 <= 15)
				dig1 += 65 - 10;
			if (0 <= dig2 && dig2 <= 9)
				dig2 += 48;
			if (10 <= dig2 && dig2 <= 15)
				dig2 += 65 - 10;

			r += '%';
			r += dig1;
			r += dig2;
		}

	return r;
}

void Http::loop (boost::asio::io_service& io_service)
{
	for (;;) {
		Request req;

		/* pop() next Request from queue_ or wait
		 * if it's empty.
		 */
		{
			boost::unique_lock<boost::mutex> lock(queue_mutex_);
			while (queue_.empty())
				queue_cond_.wait(lock);
			req = queue_.front();
			queue_.pop();
		}

		/* get/post in blocking fashion and call
		 * read handler.
		 *
		 * no need to use locking for curl (we only use it
		 * in Http::loop() thread (after ::init()))
		 */
		curl_easy_setopt(curl_, CURLOPT_URL, req.url.c_str());
		if (req.get)
			curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1);
		else {
			curl_easy_setopt(curl_, CURLOPT_POST, 1);
			curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, req.post_data.size());
			curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, req.post_data.data());
		}

		if (!req.cookies->empty()) {
			std::map<std::string, std::string>::iterator i;
			std::string cookie;
			bool fst = true;;
			for (i = req.cookies->begin(); i != req.cookies->end(); i++) {
				if (fst)
					fst = false;
				else
					cookie += "; ";
				cookie += i->first + "=" + i->second;
			}
			curl_easy_setopt(curl_, CURLOPT_COOKIE, cookie.c_str());
		}
		else
			/* clear otherwise; undocumented, may stop working... ;b */
			curl_easy_setopt(curl_, CURLOPT_COOKIE, NULL);

		boost::shared_ptr<std::string> buf(new std::string);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA,  buf.get());
		curl_easy_setopt(curl_, CURLOPT_HEADERDATA, req.cookies.get());

		int r;

		{
			boost::asio::ip::tcp::socket socket(Http::io_service());
			curl_easy_setopt(curl_, CURLOPT_SOCKOPTDATA, &socket);
			r = curl_easy_perform(curl_); /* will block */
		}

		if (!r) /* 200 OK. */
			io_service.post(boost::bind(req.handler, false, buf));
		else /* some error, copy description */ {
			*buf = err_buf_ + std::string(" (check vhost)");
			io_service.post(boost::bind(req.handler, true, buf));
		}
	}

	/* never happens */
	curl_easy_cleanup(curl_);
}

size_t Http::write_func (char* data, size_t size, size_t nmemb, std::string* buf)
{
	buf->append(data, size * nmemb);
	return size * nmemb;
}

size_t Http::headr_func (char* _data, size_t size, size_t nmemb, std::map<std::string, std::string>* cookies)
{
	const static boost::regex re("^\\s*Set-Cookie:\\s*([^=\\s]+)\\s*=\\s*([^;\\s]*)", boost::regex_constants::icase);
	std::string data(_data, size * nmemb);
	boost::smatch match;

	if (boost::regex_search(data, match, re))
		(*cookies)[match[1]] = match[2];

	return size * nmemb;
}

int Http__sockt_func (boost::asio::ip::tcp::socket* socket, curl_socket_t curlfd, curlsocktype purpose)
{
	if (Http::vhost() == "*")
		return 0;

	try {
		socket->assign(boost::asio::ip::tcp::v4(), curlfd);
		boost::asio::ip::tcp::endpoint endpoint;
		endpoint.address(boost::asio::ip::address_v4::from_string(Http::vhost()));
		socket->bind(endpoint);
	}
	catch (...) {
		die("vhost: cannot assign specified address");
	}

	return 0;
}

std::queue<Http::Request> Http::queue_;
boost::mutex Http::queue_mutex_;
boost::condition_variable Http::queue_cond_;

std::string Http::vhost_;
boost::asio::io_service* Http::io_service_(NULL);
bool Http::inited_(false);
void* Http::curl_;
char Http::err_buf_[128];

