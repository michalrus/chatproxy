#include "AuthKey.h"

#include <boost/lexical_cast.hpp>

#define AUTHKEY_e7e6279e639240569ef77fd2ede490e5 "jodua"
#define BUILD_N_e7e6279e639240569ef77fd2ede490e5 705

std::string AuthKey::getKey()
{
	return AUTHKEY_e7e6279e639240569ef77fd2ede490e5;
}

std::string AuthKey::getBuild()
{
	return boost::lexical_cast<std::string>(BUILD_N_e7e6279e639240569ef77fd2ede490e5);
}
