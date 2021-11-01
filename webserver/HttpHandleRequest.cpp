#include "HttpHandleRequest.h"
#include <fstream>

namespace {

std::string_view mime_type(std::string_view file_name)
{
    auto const ext = [&] {
        auto const pos = file_name.rfind('.');
        if (pos == std::string::npos)
            return std::string_view {};
        return file_name.substr(pos);
    }();

    auto iequals = [](std::string_view e, const char* t) {
        return e == t;
    };

    if (iequals(ext, ".htm")) return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php")) return "text/html";
    if (iequals(ext, ".css")) return "text/css";
    if (iequals(ext, ".txt")) return "text/plain";
    if (iequals(ext, ".js")) return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml")) return "application/xml";
    if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
    if (iequals(ext, ".flv")) return "video/x-flv";
    if (iequals(ext, ".png")) return "image/png";
    if (iequals(ext, ".jpe")) return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg")) return "image/jpeg";
    if (iequals(ext, ".gif")) return "image/gif";
    if (iequals(ext, ".bmp")) return "image/bmp";
    if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif")) return "image/tiff";
    if (iequals(ext, ".svg")) return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

} // namespace

HttpHandleRequest::HttpHandleRequest()
    : Object("HttpHandleRequest")
{
}

void HttpHandleRequest::setIpAddress(std::string &&ip)
{
    ip_address_ = std::move(ip);
}

std::pair<http::status, std::string> HttpHandleRequest::processRequest()
{
    log(debug) << "process request [" << ip_address_ << "] " << get().target().to_string();

    // Request must be GET ant path must be absolute
    if (get().method() != http::verb::get || get().target().find("..") != boost::beast::string_view::npos) {
        return {http::status::bad_request, "Illegal request-target"};
    }

    //
    // Parse path
    //
    {
        size_t n;
        path_stripped_ = get().target().to_string();

        n = path_stripped_.rfind('?');
        if (n != std::string::npos)
            path_stripped_ = path_stripped_.substr(0, n);

        n = path_stripped_.rfind('#');
        if (n != std::string::npos)
            path_stripped_ = path_stripped_.substr(0, n);

        if (path_stripped_.front() == '/')
            path_stripped_.erase(path_stripped_.begin(), path_stripped_.begin() + 1);

        if (path_stripped_.back() == '/')
            path_stripped_.erase(path_stripped_.end() - 1, path_stripped_.end());

        if (!path_stripped_.empty())
            pathv_ = splitString(path_stripped_, "/");
    }

    //
    // File request
    //
    std::string file_name = "html/";

    if (pathv_.size() > 1 // deny root access
        && pathv_.back().find('.') != std::string::npos
        && get().method() == http::verb::get)
    {
        file_name += path_stripped_;
    }
    else {
        file_name += "index.html";
    }

    std::ifstream in(file_name, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        log(error) << "the resource '" + file_name + "' was not found";
        return { http::status::not_found, "The resource '" + file_name + "' was not found."};
    }

    content_type_ = mime_type(file_name);

    std::string contents;
    in.seekg(0, std::ios::end);
    size_t s = in.tellg();
    contents.resize(s);
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();

    return { http::status::ok, contents};
}

path_vec HttpHandleRequest::splitString(std::string_view str, const char *delimiter)
{
    std::vector<std::string> v_;
    boost::split(v_, str, boost::is_any_of(delimiter), boost::token_compress_on);
    return v_;
}

bool HttpHandleRequest::checkPath(std::string_view format) const
{
    return format == path_stripped_;
}

bool HttpHandleRequest::checkPathPart(std::string_view format) const
{
    return format.compare(0, format.size(), path_stripped_, 0, format.size()) == 0;
}

std::string HttpHandleRequest::getCookie() const
{
    if (get().find("Cookie") != get().end()) {
        return get()["Cookie"].to_string();
    }
    return "";
}
