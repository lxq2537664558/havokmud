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
 * @brief Thread to handle logins
 */

#include <string>
#include <stdarg.h>

#include "corefunc/Logging.hpp"
#include "thread/LoginThread.hpp"
#include "corefunc/LoginStateMachine.hpp"

namespace havokmud {
    namespace thread {

        using havokmud::corefunc::LoginStateMachine;

        LoginThread::LoginThread() : InputThread("Login")
        {
            pro_initialize<LoginThread>();

            LoginStateMachine::initialize();
        }
        

        void LoginThread::start()
        {
            while (!m_abort) {
                InputQueueItem *item = m_inQueue.get();
                if (!item) {
                    continue;
                }

                Connection *connection = item->first;
                const std::string &line = item->second;
                LoginStateMachine *machine;

                ConnectionMap::iterator it = m_connectionMap.find(connection);
                if (it == m_connectionMap.end()) {
                    machine = new LoginStateMachine(*g_loginStateMachine);
                    machine->setConnection(connection);
                    m_connectionMap.insert(std::pair<Connection *,
                                    LoginStateMachine *>(connection, machine));
                } else {
                    machine = it->second;
                }

                if (!machine->handleLine(line)) {
                    removeConnection(connection);
                    connection->enterPlaying();
                }

                delete item;
            }
        }

        void LoginThread::removeConnection(Connection *connection)
        {
            ConnectionMap::iterator it = m_connectionMap.find(connection);
            if (it == m_connectionMap.end())
                return;

            delete it->second;
            m_connectionMap.erase(it);
        }
    }
}
