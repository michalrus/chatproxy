#ifndef HTTP_H_e7e6279e639240569ef77fd2ede490e5
#define HTTP_H_e7e6279e639240569ef77fd2ede490e5

#include <boost/noncopyable.hpp>
#include <curl/curl.h>
#include <string>

class Http : public boost::noncopyable {
	public:
		Http ()
			: curl_(curl_easy_init())
		{
			memset(curlErrBuf_, 0, sizeof(curlErrBuf_));
			curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, curlErrBuf_);
			curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1);
			curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.1.3) Gecko/20090824 Firefox/3.5.3");
			curl_easy_setopt(curl_, CURLOPT_COOKIEFILE, "");
		}

		~Http () {
			curl_easy_cleanup(curl_);
		}

		std::string error  () const { return curlErrBuf_; }
		std::string result () const { return buf_; }

		int get (const std::string& url) {
			curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1);

			return perform();
		}

		int post (const std::string& url, const std::string& data) {
			curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl_, CURLOPT_POST, 1);
			curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
			curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.data());

			return perform();
		}

		void file () {
			file_.erase();
		}

		void file (const std::string& path) {
			file_ = path;
		}

		static std::string urlencode (const std::string& c) {
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

	private:
		int perform () {
			FILE *fp = 0;
			buf_.erase();

			if (file_.empty()) {
				curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, curlWriteFunc);
				curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &buf_);
			}
			else {
				fp = fopen(file_.c_str(), "w");
				if (!fp) {
					snprintf(curlErrBuf_, sizeof(curlErrBuf_), "Could not open %s", file_.c_str());
					return -2;
				}
				curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, 0);
				curl_easy_setopt(curl_, CURLOPT_WRITEDATA, fp);
			}

			int r = curl_easy_perform(curl_);

			if (fp)
				fclose(fp);

			if (r)
				return -1;
			return 0;
		}

		static size_t curlWriteFunc (char* data, size_t size, size_t nmemb, std::string* buf) {
			buf->append(data, size * nmemb);
			return size * nmemb;
		}

		CURL *curl_;
		char curlErrBuf_[CURL_ERROR_SIZE];
		std::string file_, buf_;
};

#endif
