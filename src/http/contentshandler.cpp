#include "contentshandler.hpp"

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <libtorrent/hex.hpp>

#include "../sessions.hpp"

namespace fs = std::filesystem;
using porla::Http::ContentsHandler;

ContentsHandler::ContentsHandler(boost::asio::io_context &io, Sessions &sessions)
    : m_io(io), m_sessions(sessions)
{
}

void ContentsHandler::operator()(uWS::HttpResponse<false> *res, uWS::HttpRequest *req)
{
    const auto &state = m_sessions.Default();

    if (state == nullptr)
    {
        res->writeStatus("500 Internal Server Error")->end();
        return;
    }

    std::string hash(req->getParameter(0));

    if (hash.empty())
    {
        res->writeStatus("400 Bad Request")->end();
        return;
    }

    // Get our file_index parameter and try to safely parse it to an integer.
    std::string_view file_index_view(req->getParameter(1));
    int file_index;
    std::from_chars_result file_index_result =
        std::from_chars(file_index_view.data(), file_index_view.data() + file_index_view.size(), file_index);

    if (file_index_result.ec != std::errc())
    {
        res->writeStatus("400 Bad Request")->end();
        return;
    }

    // Convert our incoming hash into an info_hash_t
    // There was probably a better way to do this but
    // I have no fucking clue what I'm doing so reeee
    lt::sha1_hash h;
    lt::aux::from_hex({hash.c_str(), 40}, h.data());
    libtorrent::info_hash_t info_hash(h);

    // Find our torrent by info_hash
    auto it = state->torrents.find(info_hash);

    if (it == state->torrents.end())
    {
        res->writeStatus("404 Not Found")->end();
        return;
    }

    // Query libtorrent for only our save_path, we don't want the other data.
    auto status = it->second.status(it->second.query_save_path);

    res->onAborted([]() { 
        BOOST_LOG_TRIVIAL(fatal) << "Request was aborted";
    });

    // Boost documents an abstraction but doesn't seem to work.
    // So will need to work a solution that works on all platforms.
    // Sidenote, this gets the raw save path and doesn't exactly work
    // at the moment. But replacing the path with a large file say
    // 10GB works and the rest of porla still works.
    int fd = open(status.save_path.c_str(), O_RDONLY);
    auto descriptor = std::make_shared<boost::asio::posix::stream_descriptor>(m_io, fd);
    auto buffer = std::make_shared<std::vector<char>>(1024 * 1024); // 1 MB Buffer

    send_file_chunked(res, fd, descriptor, buffer);
}

void ContentsHandler::send_file_chunked(uWS::HttpResponse<false> *res, int fd, std::shared_ptr<boost::asio::posix::stream_descriptor> descriptor, std::shared_ptr<std::vector<char>> buffer)
{
    descriptor->async_read_some(boost::asio::buffer(*buffer), [this, res, fd, descriptor, buffer](auto ec, auto length)
    {
        if (!ec && length > 0) {
            res->write(std::string_view(buffer->data(), length));
            send_file_chunked(res, fd, descriptor, buffer);
        } else {
            close(fd);
            res->end();
        } 
    });
}