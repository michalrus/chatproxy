#ifndef EEXCEPTION_H_e7e6279e639240569ef77fd2ede490e5
#define EEXCEPTION_H_e7e6279e639240569ef77fd2ede490e5

#include <exception>
#include <string>

class EFail : public std::exception {
	public:
		EFail (const std::string& s) : s_(s) { s_.insert(0, "EFAIL: "); }
		~EFail () throw () {}
		virtual const char* what() const throw() { return s_.c_str(); }
	private:
		std::string s_;
};

#endif
