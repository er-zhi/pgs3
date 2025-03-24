#ifndef PG_CLIENT_H
#define PG_CLIENT_H

#include <stdlib.h>
#include <libpq-fe.h>
#include "s3_api.h"

// PostgreSQL client structure
typedef struct PgClient {
    char *conninfo;
    PGconn *conn;
} PgClient;

// PostgreSQL client functions
/**
 * Initialize PostgreSQL client
 * 
 * @param conninfo PostgreSQL connection string
 * @return pointer to PgClient structure or NULL if error
 */
PgClient *pg_client_init(const char *conninfo);

/**
 * Free PostgreSQL client resources
 * 
 * @param client pointer to PgClient structure
 */
void pg_client_free(PgClient *client);

// S3 API interface functions
/**
 * List all S3 buckets
 * 
 * @param client PostgreSQL client
 * @return S3Result with bucket data or NULL on error
 */
S3Result* pg_client_list_buckets(PgClient *client);

/**
 * List objects in a bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @return S3Result with object data or NULL on error
 */
S3Result* pg_client_list_objects(PgClient *client, const char *bucket);

/**
 * Get object from bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with object data or NULL on error
 */
S3Result* pg_client_get_object(PgClient *client, const char *bucket, const char *key);

/**
 * Put object in bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @param key object key
 * @param data object data
 * @param size data size
 * @param content_type content type
 * @return S3Result with status or NULL on error
 */
S3Result* pg_client_put_object(PgClient *client, const char *bucket, const char *key,
                             const void *data, size_t size, const char *content_type);

/**
 * Delete object from bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with status or NULL on error
 */
S3Result* pg_client_delete_object(PgClient *client, const char *bucket, const char *key);

#endif /* PG_CLIENT_H */ 