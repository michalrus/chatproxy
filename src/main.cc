#include <iostream>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "EException.h"
#include "Config.h"
#include "Session.h"
#include "Auth.h"

void daemonize ()
{
#ifndef BOOST_WINDOWS
	//
	// (c) Adam Wysocki <gophi@chmurka.net>
	//

	switch (fork()) {
		case 0:
			break;
		case -1:
			throw EFail("daemonize(): fork");
		default:
			std::exit (0);
	}

	if (setsid() < 0)
		throw EFail ("daemonize(): setsid");

	if (0)
		if (chdir("/"))
			throw EFail("daemonize(): chdir");

	if (1) {
		FILE *fp;

		fp = fopen("/dev/null", "w+");
		if (!fp)
			throw EFail("daemonize(): fopen(/dev/null)");

		if (dup2(fileno(fp), 0) == -1)
			throw EFail("daemonize(): dup2(0)");
		if (dup2(fileno(fp), 1) == -1)
			throw EFail("daemonize(): dup2(1)");
		if (dup2(fileno(fp), 2) == -1)
			throw EFail("daemonize(): dup2(2)");

		fclose (fp);
	}
#endif
}

void auth (Config& cnf)
{
	try {
		Auth();
	}
	catch (EFail& e) {
		cnf.errorLog() << e.what() << std::endl;
		std::exit(-1);
	}
	catch (std::exception& e) {
		cnf.errorLog() << "Auth[" << boost::this_thread::get_id() << "]: " << e.what() << std::endl;
	}
}

void handle_accept (Config& cnf, boost::shared_ptr<Session> session, boost::asio::io_service& io_service,
	boost::asio::ip::tcp::acceptor& acceptor, const boost::system::error_code& error)
{
	if (error)
		return;

	// prepare for next accept
	boost::shared_ptr<Session> newSession(new Session(cnf, io_service));
	acceptor.async_accept(newSession->tcp_.socket_, boost::bind(handle_accept, boost::ref(cnf), newSession,
		boost::ref(io_service), boost::ref(acceptor), boost::asio::placeholders::error));

	// start accepted session
	cnf.sessionAdd(session);
	session->start();
}

int main (int ac, char **av)
{
	try {
		if (ac != 1)
			throw EFail("USAGE: " + boost::filesystem::path(av[0]).filename());

		Config cnf (boost::filesystem::path(av[0]).parent_path() / "config.cnf");

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), cnf.getPort()));

		{
			// prepare for first accept
			boost::shared_ptr<Session> newSession(new Session(cnf, io_service));
			acceptor.async_accept(newSession->tcp_.socket_, boost::bind(handle_accept, boost::ref(cnf), newSession,
				boost::ref(io_service), boost::ref(acceptor), boost::asio::placeholders::error));
			// destroy newSession, so that sessionsCleanup will be able to clean it up. ^_^
		}

		std::cerr << "ENOTICE: Going into daemon mode . . . *poof*" << std::endl;
		// daemonize();

		try {
			boost::thread(auth, boost::ref(cnf));
			std::vector<boost::shared_ptr<boost::thread> > threads;

			// we need at least 2 simultaneous io_service loops
			// (Chat subclasses' dtors will block otherwise)
			for (size_t i = 0; i < cnf.getThreads(); i++) {
				boost::shared_ptr<boost::thread> tmp(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service)));
				threads.push_back(tmp);
			}

			io_service.run();

			for (size_t i = 0; i < threads.size(); i++)
				threads[i]->join();
		}
		catch (std::exception& e) {
			cnf.errorLog() << e.what() << std::endl;
			return -1;
		}
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
