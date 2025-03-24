#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdlib.h>
#include <microhttpd.h>
#include "../pg/pg_client.h"

typedef struct HttpServer {
    struct MHD_Daemon *daemon;
    PgClient *pg_client;
    int port;
} HttpServer;

/**
 * Initialize HTTP server
 * 
 * @param port port to listen on
 * @param pg_conninfo PostgreSQL connection string
 * @return pointer to HttpServer structure or NULL if error
 */
HttpServer *http_server_init(int port, const char *pg_conninfo);

/**
 * Run HTTP server (blocking call)
 * 
 * @param server pointer to HttpServer structure
 * @return 0 on success, -1 on error
 */
int http_server_run(HttpServer *server);

/**
 * Free HTTP server resources
 * 
 * @param server pointer to HttpServer structure
 */
void http_server_free(HttpServer *server);

#endif /* HTTP_SERVER_H */ 