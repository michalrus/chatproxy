#ifndef AUTH_H_e7e6279e639240569ef77fd2ede490e5
#define AUTH_H_e7e6279e639240569ef77fd2ede490e5

#include <string>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/osrng.h>

#include "EException.h"
#include "Http.h"
#include "AuthKey.h"

class Auth : public boost::noncopyable {
	public:
		Auth ()
			: connErrs_(0),
			  api_("http://cp2.michalrus.com/"),
			  connErr_    ("Could not initialize. Check your internet connection . . ."),
			  protoErr_   ("Could not initialize. Protocol error . . ."),
			  outdatedErr_("Could not initialize. This is an outdated version of chatproxy2 . . ."),
			  authErr_    ("Could not initialize. Check your license for details . . .")
		{
			return;

			std::string msg, my, his;
			byte rnd[128];

			for (;;) {
				rng_.GenerateBlock(rnd, sizeof(rnd));
				my = hash(rnd, sizeof(rnd));

				try {
					// key exchange
					if (ht_.post(api_, "k=" + my))
						throw EFail(connErr_);
					his = ht_.result();

					// proto check
					msg = my + "\nchatproxy2\n" + his;
					if (ht_.post(api_, "k=" + hash((const byte* )msg.c_str(), msg.size())))
						throw EFail(connErr_);
					msg = his + "\nok\n" + my;
					if (ht_.result() != hash((const byte* )msg.c_str(), msg.size()))
						throw EFail(protoErr_);

					// version check
					msg = my + "\n" + AuthKey::getBuild() + "\n" + his;
					if (ht_.post(api_, "k=" + hash((const byte* )msg.c_str(), msg.size()) + "&b=" + AuthKey::getBuild()))
						throw EFail(connErr_);
					msg = his + "\nok2\n" + my;
					if (ht_.result() != hash((const byte* )msg.c_str(), msg.size()))
						throw EFail(outdatedErr_);

					// auth check
					msg = my + "\n" + AuthKey::getKey() + "\n" + his;
					if (ht_.post(api_, "k=" + hash((const byte* )msg.c_str(), msg.size())))
						throw EFail(connErr_);
					msg = his + "\nok3\n" + my;
					if (ht_.result() != hash((const byte* )msg.c_str(), msg.size()))
						throw EFail(authErr_);

					connErrs_ = 0;
				}
				catch (EFail& e) {
					if (e.what() == "EFAIL: " + connErr_) {
						if (++connErrs_ > 1)
							throw;
						else
							boost::this_thread::sleep(boost::posix_time::seconds(120));
					}
				}

				boost::this_thread::sleep(boost::posix_time::seconds(60));
			}
		}

	private:
		static std::string hash (const byte* bl, size_t size)
		{
			std::string r;

			CryptoPP::SHA512 h;
			byte digest[CryptoPP::SHA512::DIGESTSIZE];
			h.CalculateDigest(digest, bl, size);

			CryptoPP::HexEncoder encoder;
			encoder.Attach(new CryptoPP::StringSink(r));
			encoder.Put(digest, sizeof(digest));
			encoder.MessageEnd();

			boost::to_lower(r);
			return r;
		}

		size_t connErrs_;
		CryptoPP::AutoSeededRandomPool rng_;
		Http ht_;
		const std::string api_, connErr_, protoErr_, outdatedErr_, authErr_;
};

#endif
