#ifndef SOCKET_SERVER_INTERNAL_H
#define SOCKET_SERVER_INTERNAL_H

#include "server.h"

int server_socket_open(const ServerConfig *config, SqlError *error);
int server_socket_accept_loop(DbServer *server, SqlError *error);
void server_socket_close(int *listen_fd);

#endif
