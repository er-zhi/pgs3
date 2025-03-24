#include "pg_client.h"
#include <string.h>
#include <stdio.h>

/**
 * Initialize PostgreSQL client
 * 
 * @param conninfo PostgreSQL connection string
 * @return pointer to PgClient structure or NULL if error
 */
PgClient *pg_client_init(const char *conninfo) {
    if (!conninfo) {
        return NULL;
    }
    
    PgClient *client = (PgClient *)malloc(sizeof(PgClient));
    if (!client) {
        return NULL;
    }
    
    client->conninfo = strdup(conninfo);
    client->conn = PQconnectdb(conninfo);
    
    if (PQstatus(client->conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(client->conn));
        pg_client_free(client);
        return NULL;
    }
    
    return client;
}

/**
 * Free PostgreSQL client resources
 * 
 * @param client pointer to PgClient structure
 */
void pg_client_free(PgClient *client) {
    if (!client) {
        return;
    }
    
    if (client->conn) {
        PQfinish(client->conn);
    }
    
    if (client->conninfo) {
        free(client->conninfo);
    }
    
    free(client);
}

// S3 API interface functions

/**
 * List all S3 buckets
 * 
 * @param client PostgreSQL client
 * @return S3Result with bucket data or NULL on error
 */
S3Result* pg_client_list_buckets(PgClient *client) {
    if (!client || !client->conn) {
        return NULL;
    }
    
    return s3_api_list_buckets(client->conn);
}

/**
 * List objects in a bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @return S3Result with object data or NULL on error
 */
S3Result* pg_client_list_objects(PgClient *client, const char *bucket) {
    if (!client || !client->conn || !bucket) {
        return NULL;
    }
    
    return s3_api_list_objects(client->conn, bucket);
}

/**
 * Get object from bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with object data or NULL on error
 */
S3Result* pg_client_get_object(PgClient *client, const char *bucket, const char *key) {
    if (!client || !client->conn || !bucket || !key) {
        return NULL;
    }
    
    return s3_api_get_object(client->conn, bucket, key);
}

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
                             const void *data, size_t size, const char *content_type) {
    if (!client || !client->conn || !bucket || !key || !data || size == 0) {
        return NULL;
    }
    
    return s3_api_put_object(client->conn, bucket, key, data, size, content_type);
}

/**
 * Delete object from bucket
 * 
 * @param client PostgreSQL client
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with status or NULL on error
 */
S3Result* pg_client_delete_object(PgClient *client, const char *bucket, const char *key) {
    if (!client || !client->conn || !bucket || !key) {
        return NULL;
    }
    
    return s3_api_delete_object(client->conn, bucket, key);
} 