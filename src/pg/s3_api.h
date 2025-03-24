#ifndef S3_API_H
#define S3_API_H

#include <stdlib.h>
#include <libpq-fe.h>

/**
 * S3 result status enum
 */
typedef enum {
    S3_SUCCESS,
    S3_ERROR_CONNECTION,
    S3_ERROR_EXECUTION,
    S3_ERROR_NOT_FOUND,
    S3_ERROR_PERMISSION,
    S3_ERROR_INVALID_INPUT,
    S3_ERROR_MEMORY
} S3StatusEnum;

/**
 * S3 result structure
 */
typedef struct S3Result {
    S3StatusEnum status;
    char *content_type;
    void *data;
    size_t data_size;
    char *error_message;
} S3Result;

/**
 * Create a new S3Result
 * 
 * @return pointer to S3Result or NULL on error
 */
S3Result* s3_result_create(void);

/**
 * Free S3Result resources
 * 
 * @param result pointer to S3Result
 */
void s3_result_free(S3Result *result);

/**
 * Set error in S3Result
 * 
 * @param result pointer to S3Result
 * @param status error status
 * @param message error message
 */
void s3_result_set_error(S3Result *result, S3StatusEnum status, const char *message);

/**
 * List all buckets
 * 
 * @param conn PostgreSQL connection
 * @return S3Result with bucket data
 */
S3Result* s3_api_list_buckets(PGconn *conn);

/**
 * List objects in a bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @return S3Result with object data
 */
S3Result* s3_api_list_objects(PGconn *conn, const char *bucket);

/**
 * Get object from bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with object data
 */
S3Result* s3_api_get_object(PGconn *conn, const char *bucket, const char *key);

/**
 * Put object in bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @param key object key
 * @param data object data
 * @param size data size
 * @param content_type content type
 * @return S3Result with status
 */
S3Result* s3_api_put_object(PGconn *conn, const char *bucket, const char *key,
                          const void *data, size_t size, const char *content_type);

/**
 * Delete object from bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with status
 */
S3Result* s3_api_delete_object(PGconn *conn, const char *bucket, const char *key);

#endif /* S3_API_H */ 