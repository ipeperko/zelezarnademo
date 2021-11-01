#ifndef HTTPHANDLEREQUEST_H
#define HTTPHANDLEREQUEST_H

#include "Object.h"
#include "Webserver_common.h"

using path_vec = std::vector<std::string>;

class HttpHandleRequest : public Object, public http::request_parser<http::string_body>
{
public:
    HttpHandleRequest();

    void setIpAddress(std::string&& ip);

    template<class Send>
    void handle_request(Send&& send);

private:

    void sendResponse(std::string&& content);

    std::pair<http::status, std::string> processRequest();

    static path_vec splitString(std::string_view str, const char *delimiter);

    bool checkPath(std::string_view format) const;

    template <class... T>
    bool checkPath(std::string_view format, T&... parms) const;

    bool checkPathPart(std::string_view format) const;

    template <class... T>
    bool checkPathPart(std::string_view format, T&... parms) const;

    std::string getCookie() const;

    std::string ip_address_;
    std::string path_stripped_;
    path_vec pathv_;
    std::string content_type_;
};

template <class... T>
bool HttpHandleRequest::checkPath(std::string_view format, T&... parms) const {

    size_t n_numbers = 0;
    size_t n_strings = 0;
    path_vec formatv_ = splitString(format, "/");

    if (pathv_.size() != formatv_.size()) {
        return false;
    }

    for (size_t i=0; i<pathv_.size(); i++) {

        std::string_view pi = pathv_[i];
        std::string_view fi = formatv_[i];

        if (fi == "%d") {
            n_numbers++;
        } else if (fi == "%s") {
            n_strings++;
        } else if (pi != fi) {
            return false;
        }
    }

    size_t n_scanned = sscanf(path_stripped_.c_str(), format, &parms...);
    size_t n_parms = sizeof...(T);

    if (n_scanned != n_parms || n_scanned != (n_numbers + n_strings)) {
        //std::cout << "Num all numbers parsed\n";
        return false;
    }

    return true;
}

template <class... T>
bool HttpHandleRequest::checkPathPart(std::string_view format, T&... parms) const
{
    size_t n_numbers = 0;
    size_t n_strings = 0;
    path_vec formatv_ = splitString(format, "/");

    if (pathv_.size() < formatv_.size()) {
        return false;
    }

    for (size_t i=0; i<formatv_.size(); i++) {

        std::string_view pi = pathv_[i];
        std::string_view fi = formatv_[i];

        if (fi == "%d") {
            n_numbers++;
        } else if (fi == "%s") {
            n_strings++;
        } else if (pi != fi) {
            return false;
        }
    }

    size_t n_scanned = sscanf(path_stripped_.c_str(), format.data(), &parms...);
    size_t n_parms = sizeof...(T);

    if (n_scanned != n_parms || n_scanned != (n_numbers + n_strings)) {
        //std::cout << "Num all numbers parsed\n";
        return false;
    }

    return true;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Send>
void HttpHandleRequest::handle_request(Send&& send)
{
    // Returns a server error response
    auto const server_error =
            [this](boost::beast::string_view what, http::status ec = http::status::internal_server_error)
            {
                auto& req = *this;
                http::response<http::string_body> res{ec, req.get().version()};
                res.set(http::field::server, WebserverVersion::get());
                res.set(http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = what.to_string();
                res.prepare_payload();
                return res;
            };

    http::string_body::value_type body;
    http::status err_code;

    try {
        std::tie(err_code, body) = processRequest();

        if (err_code != http::status::ok) {
            send(server_error(body, err_code));
            return;
        }

    } catch(const std::exception& e) {
        log(error) << e.what();
        send(server_error(e.what()));
        return;
    }

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to GET request
    http::response<http::string_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, get().version())};
    res.set(http::field::server, WebserverVersion::get());
    res.set(http::field::content_type, content_type_);
    res.content_length(size);
    res.keep_alive(keep_alive());
    send(std::move(res));
}

#endif //HTTPHANDLEREQUEST_H
