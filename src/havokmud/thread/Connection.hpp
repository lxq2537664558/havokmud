/*
 *  This file is part of the havokmud package
 *  Copyright (C) 2013 Gavin Hurlbut
 *
 *  havokmud is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file
 * @brief Thread to handle network connections.
 */

#ifndef __havokmud_thread_Connection__
#define __havokmud_thread_Connection__

#include <boost/circular_buffer.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>

#include "HavokThread.hpp"

#define IS_POWER_2(x) ((x) != 0 && !((x) & ((x) - 1))

namespace havokmud {
    namespace objects {

        #define MAX_BUFSIZE 8192

        class Player;

        using boost::asio::ip::tcp;

        class Connection :
                public boost::enable_shared_from_this<Connection>
        {
        public:
            typedef boost::shared_ptr<Connection> pointer;

            static pointer create(boost::asio::io_service &io_service,
                                  Player *player;
                                  unsigned int inBufferSize = MAX_BUFSIZE)
            {
                return pointer(new Connection(io_service, player,
                                              inBufferSize));
            };

            ~Connection();

            tcp::socket &socket() const  { return m_socket; };
            Player *player() const  { return m_player; };

            void handle_read(const boost::system::error_code &e,
                             std::size_t bytes_transferred);
            void handle_write(const boost::system::error_code &e);

            void send(boost::asio::buffer buffer);

        private:
            Connection(boost::asio::io_service &io_service, Player *player,
                       unsigned int inBufferSize = MAX_BUFSIZE);

            void setHostname(std::string hostname) { m_hostname = hostname; };
            void sendBuffers();

            unsigned char *splitLines(boost::asio::buffer &inBuffer,
                                      boost::asio::buffer &remainBuf);

            tcp::socket                     m_socket;
            Player                         *m_player;
            boost::asio::mutable_buffer     m_inBufRemain;
            unsigned char                  *m_inBufRaw;
            boost::asio::mutable_buffer     m_inBuf;
            std::vector<boost::asio::const_buffer>  m_outBufVector;
            std::string                     m_hostname;

            bool                            m_writing;
        };

        using havokmud::thread::HavokThread;

        class ConnectionThread : public HavokThread
        {
        public:
            ConnectionThread(int port, struct timeval timeout) :
                    HavokThread("Connection"), m_port(port), m_count(0),
                    m_fdCount(0), m_timeout(timeout)  {};
            ~ConnectionThread();

        private:
            int                     m_port;
            int                     m_count;
            int                     m_fdCount;
            struct timeval          m_timeout;
        };
    }
}

#endif  // __havokmud_thread_Connection__
