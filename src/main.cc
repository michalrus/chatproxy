/**
 * chatproxy3 v3.0
 * Copyright (C) 2011  Michal Rus
 * http://michalrus.com/code/chatproxy3/
 *
 * chatproxy3 lets you connect to various applet-based
 * webchat services using any ordinary IRC client
 *
 * main.cc -- entry point, session acceptor and demonizer.
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

/**
 * System specific includes, boost etc.
 */

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#ifdef BOOST_WINDOWS
#	include <windows.h>
#endif

#include <string>

#include "Debug.h"
#include "Session.h"
#include "Config.h"
#include "Http.h"

/**
 * Daemonizing procedure for UNIX.
 *
 * Taken from <http://www.chmurka.net/p/ircproxy.c>
 * Credit goes to Adam Wysocki <gophi@chmurka.net>.
 */
void daemonize ()
{
#ifndef BOOST_WINDOWS
	switch (fork()) {
		case 0:
			break;
		case -1:
			die("fork");
		default:
			std::exit (0);
	}

	if (setsid() < 0)
		die("setsid");

	if (0 && chdir("/"))
		die("chdir");

	if (1) {
		FILE *fp;
		int rs = 0;

		fp = fopen("/dev/null", "w+");
		if (!fp)
			die("fopen(/dev/null)");

		rs |= dup2(fileno(fp), 0) == -1 ? 1 : 0;
		rs |= dup2(fileno(fp), 1) == -1 ? 1 : 0;
		rs |= dup2(fileno(fp), 2) == -1 ? 1 : 0;

		if (rs)
			die("dup2");

		fclose (fp);
	}
#endif
}

/**
 * Async. accept handler.
 */
void handle_accept (Config& cnf, boost::shared_ptr<Session> session, boost::asio::io_service& io_service,
	boost::asio::ip::tcp::acceptor& acceptor, const boost::system::error_code& error)
{
	if (error)
		return;

	/* prepare for next accept */
	boost::shared_ptr<Session> new_sess(new Session(cnf, io_service));
	acceptor.async_accept(new_sess->tcp_.socket(), boost::bind(handle_accept, boost::ref(cnf), new_sess,
		boost::ref(io_service), boost::ref(acceptor), boost::asio::placeholders::error));

	/* keep a reference to @session shared_ptr -- it will otherwise
	 * get freed after Session::start() below
	 */
	cnf.sessions_.push_back(session);

	/* start accepted session */
	session->start();
}

/**
 * Entry point.
 *
 * main() and daemonize() on UNIX
 * WinMain() and no CreateWindow() on Windows
 */
#ifdef BOOST_WINDOWS
int WINAPI WinMain (HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow)
#else
int main (int ac, char** av)
#endif
{
	/* where are we? */
	boost::filesystem::path path;
#ifndef BOOST_WINDOWS
	path = av[0];
#else
	{
		char buf[MAX_PATH];
		GetModuleFileName(NULL, buf, sizeof(buf));
		path = buf;
	}
#endif

	/* we want argc to be 0 */
#ifndef BOOST_WINDOWS
	if (ac != 1)
#else
	if (std::string(lpszArgument).length())
#endif
		die("usage: " + path.filename());

	/* no need to remember exec path */
	path = path.parent_path();

	/* io_service */
	boost::asio::io_service io_service;

	/* read config */
	Config cnf(path);
	try {
		cnf.read(path / "chatproxy3.conf");
	}
	catch (...) {
		die("could not read " + (path / "chatproxy3.conf").string());
	}

	bool open_errlog = true;
	/* display daemonizing notice before opening error log */
#ifndef BOOST_WINDOWS
	if (cnf.get<std::string>("daemonize") == "yes")
		debug("daemonizing");
	else {
		debug("not daemonizing; all debug data will be sent to stderr");
		open_errlog = false;
	}
#else
	/* a more verbose one for clickers ;p */
	debug("\n\nchatproxy3 runs in the background.\nYou can see (and kill) it in your process list: [Ctrl]+[Shift]+[Esc].\n\nIn your IRC client, type:\n    /connect localhost " + cnf.get<std::string>("listen"));
#endif

	if (open_errlog) {
		/* open error log; from now on, all debug notices will
		 * be saved to it
		 */
		try {
			Debug::open_errlog(path / "chatproxy3.log");
		}
		catch (...) {
			die("could not open " + (path / "chatproxy3.log").string());
		}
	}

	/* time to daemonize if we're at UNIX; we need to do
	 * this *before* starting new thread in Config::Config();
	 */
#ifndef BOOST_WINDOWS
	if (cnf.get<std::string>("daemonize") == "yes")
		daemonize();
#endif

	/* resolve "vhost" and "bind" */
	{
		static const char* to_res[] = { "vhost", "bind", NULL };
		boost::asio::ip::tcp::resolver resolver(io_service);
		for (size_t i = 0; to_res[i]; i++) {
			std::string dir(cnf.get<std::string>(to_res[i]));
			if (dir == "*")
				continue;
			try {
				boost::asio::ip::tcp::resolver::iterator iter
					= resolver.resolve(boost::asio::ip::tcp::resolver::query(dir, ""));
				cnf.set(to_res[i], iter->endpoint().address().to_v4().to_string());
			}
			catch (...) {
				die("could not resolve \"" + std::string(to_res[i]) + "\" directive (\"" + dir + "\") to IPv4 address");
			}
		}
	}

	try {
		/* bind */
		boost::asio::ip::tcp::endpoint endpoint;
		{
			std::string local_end(cnf.get<std::string>("bind"));
			if (local_end == "*")
				endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), cnf.get<unsigned short>("listen"));
			else
				endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string(local_end), cnf.get<unsigned short>("listen"));
		}
		boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);

		/* prepare for first accept */
		{
			boost::shared_ptr<Session> new_sess(new Session(cnf, io_service));
			acceptor.async_accept(new_sess->tcp_.socket(), boost::bind(handle_accept, boost::ref(cnf), new_sess,
				boost::ref(io_service), boost::ref(acceptor), boost::asio::placeholders::error));
		}

		/* start Http::loop() thread */
		Http::init(io_service, cnf.get<std::string>("vhost"));

		/* `unbrot' signatures */
		Session::brot(Session::signature_onl_, -113);
		Session::brot(Session::signature_app_, -113);

		/* main loop */
		io_service.run();
	}
	catch (std::exception& e) {
		die(e.what());
	}

	/* never happens */
	return 0;
}
