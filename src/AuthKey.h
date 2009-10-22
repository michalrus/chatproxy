#ifndef AUTHKEY_H_e7e6279e639240569ef77fd2ede490e5
#define AUTHKEY_H_e7e6279e639240569ef77fd2ede490e5

#include <string>
#include <boost/noncopyable.hpp>

class AuthKey : public boost::noncopyable {
	public:
		static std::string getKey();
		static std::string getBuild();
};

#endif
