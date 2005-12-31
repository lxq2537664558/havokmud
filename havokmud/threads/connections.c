/*
 *  This file is part of the havokmud package
 *  Copyright (C) 2005 Gavin Hurlbut
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

/*HEADER---------------------------------------------------
 * $Id$
 *
 * Copyright 2005 Gavin Hurlbut
 * All rights reserved
 *
 * Comments :
 *
 * Thread to handle network connections.
 */

#include "environment.h"
#include <pthread.h>
#include "protos.h"
#include "externs.h"
#include "interthread.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include "linked_list.h"
#include "buffer.h"
#include "queue.h"
#include "logging.h"

static char ident[] _UNUSED_ =
    "$Id$";

static int listenFd = -1;
static int maxFd = -1;


#define MAX_BUFSIZE 8192


LinkedList_t *ConnectionList = NULL;
static int recalcMaxFd = FALSE;
static fd_set readFds, writeFds, exceptFds;
static fd_set saveReadFds, saveWriteFds, saveExceptFds;

static ConnectionItem_t *connRemove(ConnectionItem_t *item);
static void connAddFd( int fd, fd_set *fds );

void *ConnectionThread( void *arg )
{
    connectThreadArgs_t *argStruct;
    int portNum;
    struct sockaddr_in sa;
    int count;
    int fdCount;
    int newFd;
    socklen_t salen;
    struct timeval timeout;
    ConnectionItem_t *item;
    PlayerStruct_t *player;
    ConnInputItem_t *connItem;
    ConnDnsItem_t *dnsItem;
    uint32 i;

    argStruct = (connectThreadArgs_t *)arg;
    portNum = argStruct->port;

    LogPrint( LOG_NOTICE, "Starting Connection Thread, port %d", portNum );

    /*
     * Start listening
     */

    listenFd = socket( PF_INET, SOCK_STREAM, 0 );
    if( listenFd < 0 ) {
        perror("Opening listener socket");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portNum);

    if (bind(listenFd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("Binding listener socket");
        close(listenFd);
        exit(1);
    }

    if (listen(listenFd, 10)) {
        perror("Listening to socket");
        close(listenFd);
        exit(1);
    }

    FD_ZERO(&saveReadFds);
    FD_ZERO(&saveWriteFds);
    FD_ZERO(&saveExceptFds);

    connAddFd(listenFd, &saveReadFds);

    ConnectionList = LinkedListCreate();
    LogPrint( LOG_DEBUG, "ConnectionList = %p", ConnectionList );

    while( 1 ) {
        /*
         * Select on connected and listener
         */
        readFds   = saveReadFds;
        writeFds  = saveWriteFds;
        exceptFds = saveExceptFds;
        timeout.tv_sec  = argStruct->timeout_sec;
        timeout.tv_usec = argStruct->timeout_usec;
        
        fdCount = select(maxFd+1, &readFds, &writeFds, &exceptFds, &timeout);

        recalcMaxFd = FALSE;

        /*
         * Open a connection for listener
         */
        if( FD_ISSET(listenFd, &readFds) ) {
            newFd = accept(listenFd, (struct sockaddr *)&sa, &salen);

            connAddFd(newFd, &saveReadFds);
            connAddFd(newFd, &saveExceptFds);

            item = (ConnectionItem_t *)malloc(sizeof(ConnectionItem_t));
            if( !item ) {
                /*
                 * No memory!
                 */
                LogPrintNoArg( LOG_EMERG, "Out of memory!" );
                close(newFd);
            } else {
                memset(item, 0, sizeof(ConnectionItem_t));
                item->fd = newFd;
                item->buffer = BufferCreate(MAX_BUFSIZE);

                item->hostName = ProtectedDataCreate();
                ProtectedDataLock( item->hostName );
                item->hostName->data = malloc(16);
                i = sa.sin_addr.s_addr;
                sprintf((char *)item->hostName->data, "%d.%d.%d.%d", 
                        (i & 0x000000FF), (i & 0x0000FF00) >> 8, 
                        (i & 0x00FF0000) >> 16, (i & 0xFF000000) >> 24);
                ProtectedDataUnlock( item->hostName );

                if (!IS_SET(SystemFlags, SYS_SKIPDNS)) {
                    dnsItem = (ConnDnsItem_t *)malloc(sizeof(ConnDnsItem_t));
                    if( dnsItem ) {
                        dnsItem->connection = item;
                        dnsItem->ipAddr = sa.sin_addr.s_addr;
                        QueueEnqueueItem(ConnectDnsQ, dnsItem);
                    }
                }

                player = (PlayerStruct_t *)malloc(sizeof(PlayerStruct_t));
                if( !player ) {
                    /*
                     * No memory!
                     */
                    LogPrintNoArg( LOG_EMERG, "Out of memory!" );
                    BufferDestroy(item->buffer);
                    close(newFd);
                    free(item);
                } else {
                    memset(player, 0, sizeof(PlayerStruct_t));
                    item->player = player;
                    player->connection = item;
                    player->in_buffer = item->buffer;

                    LinkedListAdd( ConnectionList, (LinkedListItem_t *)item, 
                                   UNLOCKED, AT_TAIL );
                    /*
                     * Pass the info on to the other threads...
                     */
                    connItem = (ConnInputItem_t *)
                                  malloc(sizeof(ConnInputItem_t));
                    if( connItem ) {
                        connItem->type = CONN_NEW_CONNECT;
                        connItem->player = player;

                        QueueEnqueueItem(ConnectInputQ, (QueueItem_t)connItem);
                    }
                }
            }

            fdCount--;
        }

        if( fdCount ) {
            LinkedListLock( ConnectionList );

            for( item = (ConnectionItem_t *)(ConnectionList->head); 
                   item && fdCount; 
                   item = (item ? (ConnectionItem_t *)item->link.next
                                : (ConnectionItem_t *)ConnectionList->head) ) {
                if( FD_ISSET( item->fd, &exceptFds ) ) {
                    /*
                     * This connection's borked, close it, remove it, move on
                     */
                    if( FD_ISSET( item->fd, &readFds ) ) {
                        fdCount--;
                    }

                    if( FD_ISSET( item->fd, &writeFds ) ) {
                        fdCount--;
                    }

                    BufferLock( item->buffer );
                    item = connRemove(item);
                    fdCount--;
                    continue;
                }

                if( item && FD_ISSET( item->fd, &readFds ) ) {
                    /*
                     * This connection has data ready
                     */
                    count = BufferAvailWrite( item->buffer, TRUE );
                    if( !count ) {
                        /*
                         * No buffer space, the buffer's unlocked, move on
                         */
                        continue;
                    }

                    /*
                     * The buffer's locked
                     */
                    count = read( item->fd, BufferGetWrite( item->buffer ),
                                  count );
                    if( !count ) {
                        /*
                         * We hit EOF, close and remove
                         */
                        if( FD_ISSET( item->fd, &writeFds ) ) {
                            fdCount--;
                        }

                        item = connRemove(item);
                        fdCount--;
                        continue;
                    }

                    BufferWroteBytes( item->buffer, count );
                    BufferUnlock( item->buffer );

                    /*
                     * Tell the input thread
                     */
                    connItem = (ConnInputItem_t *)
                                  malloc(sizeof(ConnInputItem_t));
                    if( connItem ) {
                        connItem->type = CONN_INPUT_AVAIL;
                        connItem->player = item->player;

                        QueueEnqueueItem(ConnectInputQ, (QueueItem_t)connItem);
                    }
                }

                if( item && FD_ISSET( item->fd, &writeFds ) ) {
                    /*
                     * We have space to output, so write if we have anything
                     * to write
                     */
                    if( item->outBufDesc ) {
                        write( item->fd, item->outBufDesc->buf, 
                               item->outBufDesc->len );
                        free( item->outBufDesc->buf );
                        free( item->outBufDesc );
                    }
                    /*
                     * Kick the next output
                     */
                    connKickOutput( item );
                    fdCount--;
                }
            }
            LinkedListUnlock( ConnectionList );
        }

        if( recalcMaxFd ) {
            LinkedListLock( ConnectionList );
            maxFd = listenFd;

            for( item = (ConnectionItem_t *)(ConnectionList->head); item; 
                 item = (ConnectionItem_t *)item->link.next ) {
                if( item->fd > maxFd ) {
                    maxFd = item->fd;
                }
            }
            LinkedListUnlock( ConnectionList );
        }
    }

    return( NULL );
}

static void connAddFd( int fd, fd_set *fds )
{
    FD_SET(fd, fds);
    if( fd > maxFd ) {
        maxFd = fd;
    }
}

static void connDelFd( int fd, fd_set *fds )
{
    FD_CLR(fd, fds);
}

static ConnectionItem_t *connRemove(ConnectionItem_t *item)
{
    ConnectionItem_t *prev;
    PlayerStruct_t *player;
    ConnInputItem_t *connItem;

    close(item->fd);
    connDelFd( item->fd, &saveReadFds );
    connDelFd( item->fd, &saveWriteFds );
    connDelFd( item->fd, &saveExceptFds );

    player = item->player;

    if( item->link.prev ) {
        item->link.prev->next = item->link.next;
    } else {
        ConnectionList->head = item->link.next;
    }

    if( item->link.next ) {
        item->link.next->prev = item->link.prev;
    } else {
        ConnectionList->tail = item->link.prev;
    }

    prev = (ConnectionItem_t *)(item->link.prev);

    /*
     * must be locked before entering...
     */
    BufferDestroy( item->buffer );
    free( item );

    connItem = (ConnInputItem_t *)malloc(sizeof(ConnInputItem_t));
    if( connItem ) {
        connItem->type = CONN_DELETE_CONNECT;
        connItem->player = player;

        /*
         * Ignore any remaining input
         */
        player->flush = TRUE;

        QueueEnqueueItem(ConnectInputQ, (QueueItem_t)connItem);
    }

    recalcMaxFd = TRUE;

    return( prev );
}

void connClose( ConnectionItem_t *connItem )
{
    LinkedListLock( ConnectionList );
    connRemove( connItem );
    LinkedListUnlock( ConnectionList );
}

void connKickOutput( ConnectionItem_t *connItem )
{
    PlayerStruct_t *player;
    OutputBuffer_t *bufDesc;

    if( !connItem ) {
        return;
    }

    player = connItem->player;

    bufDesc = (OutputBuffer_t *)QueueDequeueItem( player->outputQ, 0 );
    if( !bufDesc ) {
        if( !connItem->outBufDesc ) {
            connDelFd( connItem->fd, &saveWriteFds );
        }
        connItem->outBufDesc = NULL;
    } else {
        connAddFd( connItem->fd, &saveWriteFds );
        connItem->outBufDesc = bufDesc;
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */