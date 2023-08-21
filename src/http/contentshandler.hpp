#pragma once

#include <boost/asio.hpp>

#include <map>
#include <string>
#include <vector>

#include "handler.hpp"
#include "../sessions.hpp"

namespace porla
{
    class Sessions;
}

namespace porla::Http
{
    class ContentsHandler
    {
    public:
        explicit ContentsHandler(boost::asio::io_context &io, Sessions &sessions);

        void send_file_chunked(uWS::HttpResponse<false> *res, int fd, std::shared_ptr<boost::asio::posix::stream_descriptor> descriptor, std::shared_ptr<std::vector<char>> buffer);

        void operator()(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);

    private:
        boost::asio::io_context &m_io;
        Sessions &m_sessions;
    };
}
