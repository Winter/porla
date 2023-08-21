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

        void send_file_async(uWS::HttpResponse<false> *res, const std::string &path);

        void
            operator()(uWS::HttpResponse<false> *res, uWS::HttpRequest *req);

    private:
        boost::asio::io_context &m_io;
        boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
        Sessions &m_sessions;
    };
}
