#include "contentshandler.hpp"

#include <filesystem>
#include <regex>

#include <boost/asio.hpp>
#include <boost/asio/basic_file.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <fstream>
#include <libtorrent/hex.hpp>
#include <thread>
#include <utility>
#include <zip.h>

#include "../sessions.hpp"

namespace fs = std::filesystem;
using porla::Http::ContentsHandler;

ContentsHandler::ContentsHandler(boost::asio::io_context &io, Sessions &sessions)
    : m_io(io), m_sessions(sessions), m_strand(boost::asio::make_strand(io))
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

    send_file_async(res, status.save_path);
}

// Doesn't actually work without blocking the main thread.. 
// Will maybe need to try another approach. Was just a first attempt.
void ContentsHandler::send_file_async(uWS::HttpResponse<false> *res, const std::string &path)
{
    auto file = std::make_shared<std::ifstream>(path, std::ios::binary);

    if (!file->is_open())
    {
        res->writeStatus("500 Internal Server Error")->end();
        return;
    }

    // uWS complains without this
    res->onAborted([]() { 
        std::cout << "Request was aborted before response was completed." << std::endl; 
    });

    std::thread([this, res, file]
    {
        std::array<char, 8192> buffer;  // 8 KB buffer

        while (file->read(buffer.data(), buffer.size()) || file->gcount() != 0) {
            std::size_t bytes_read = file->gcount();
            
            auto buffer_copy = std::make_shared<std::array<char, 8192>>(buffer);

            boost::asio::post(m_strand, [res, buffer_copy, bytes_read] {
                res->write(std::string_view(buffer_copy->data(), bytes_read));
            });
        }

        boost::asio::post(m_strand, [res] {
            res->end();
        });

        file->close(); 
    })
    .detach();
}