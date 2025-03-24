#include "s3_api.h"
#include <string.h>
#include <stdio.h>

/**
 * Create a new S3Result
 * 
 * @return pointer to S3Result or NULL on error
 */
S3Result* s3_result_create(void) {
    S3Result *result = (S3Result *)malloc(sizeof(S3Result));
    if (!result) {
        return NULL;
    }
    
    memset(result, 0, sizeof(S3Result));
    result->status = S3_SUCCESS;
    
    return result;
}

/**
 * Free S3Result resources
 * 
 * @param result pointer to S3Result
 */
void s3_result_free(S3Result *result) {
    if (!result) {
        return;
    }
    
    if (result->data) {
        free(result->data);
    }
    
    if (result->content_type) {
        free(result->content_type);
    }
    
    if (result->error_message) {
        free(result->error_message);
    }
    
    free(result);
}

/**
 * Set error in S3Result
 * 
 * @param result pointer to S3Result
 * @param status error status
 * @param message error message
 */
void s3_result_set_error(S3Result *result, S3StatusEnum status, const char *message) {
    if (!result) {
        return;
    }
    
    result->status = status;
    
    if (result->error_message) {
        free(result->error_message);
    }
    
    if (message) {
        result->error_message = strdup(message);
    } else {
        result->error_message = NULL;
    }
}

/**
 * Helper function to check PG connection and execute a query
 * 
 * @param conn PostgreSQL connection
 * @param query SQL query
 * @return PGresult or NULL on error
 */
static PGresult* execute_query(PGconn *conn, const char *query) {
    if (!conn || !query) {
        return NULL;
    }
    
    if (PQstatus(conn) != CONNECTION_OK) {
        return NULL;
    }
    
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return NULL;
    }
    
    return res;
}

/**
 * Ensure S3 schema and tables exist in the database
 * 
 * @param conn PostgreSQL connection
 * @return 0 on success, -1 on error
 */
static int ensure_s3_schema(PGconn *conn) {
    if (!conn) {
        return -1;
    }
    
    // Create schema if not exists
    const char *create_schema = 
        "CREATE SCHEMA IF NOT EXISTS s3;";
    
    PGresult *res = execute_query(conn, create_schema);
    if (!res) {
        return -1;
    }
    PQclear(res);
    
    // Create objects table if not exists
    const char *create_objects = 
        "CREATE TABLE IF NOT EXISTS s3.objects ("
        "   path TEXT PRIMARY KEY,"
        "   content BYTEA NOT NULL,"
        "   content_type TEXT NOT NULL,"
        "   size BIGINT NOT NULL,"
        "   last_modified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    res = execute_query(conn, create_objects);
    if (!res) {
        return -1;
    }
    PQclear(res);
    
    return 0;
}

/**
 * List all buckets
 * 
 * @param conn PostgreSQL connection
 * @return S3Result with bucket data
 */
S3Result* s3_api_list_buckets(PGconn *conn) {
    S3Result *result = s3_result_create();
    if (!result) {
        return NULL;
    }
    
    if (!conn) {
        s3_result_set_error(result, S3_ERROR_CONNECTION, "Invalid PostgreSQL connection");
        return result;
    }
    
    // We only support a single bucket named "public"
    const char *buckets_json = "["
        "{\"Name\":\"public\",\"CreationDate\":\"2023-01-01T00:00:00.000Z\"}"
    "]";
    
    result->data = strdup(buckets_json);
    result->data_size = strlen(buckets_json);
    result->content_type = strdup("application/json");
    
    return result;
}

/**
 * List objects in a bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @return S3Result with object data
 */
S3Result* s3_api_list_objects(PGconn *conn, const char *bucket) {
    S3Result *result = s3_result_create();
    if (!result) {
        return NULL;
    }
    
    if (!conn) {
        s3_result_set_error(result, S3_ERROR_CONNECTION, "Invalid PostgreSQL connection");
        return result;
    }
    
    if (!bucket) {
        s3_result_set_error(result, S3_ERROR_INVALID_INPUT, "Bucket name is required");
        return result;
    }
    
    // Check if the bucket is "public" (the only supported bucket)
    if (strcmp(bucket, "public") != 0) {
        s3_result_set_error(result, S3_ERROR_NOT_FOUND, "Bucket not found");
        return result;
    }
    
    // Ensure schema and tables exist
    if (ensure_s3_schema(conn) != 0) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to ensure schema");
        return result;
    }
    
    // Query objects from database
    const char *query = 
        "SELECT path, size, to_char(last_modified, 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') as lastmod "
        "FROM s3.objects "
        "ORDER BY path;";
    
    PGresult *res = execute_query(conn, query);
    if (!res) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to query objects");
        return result;
    }
    
    int rows = PQntuples(res);
    if (rows == 0) {
        // No objects found, return empty array
        const char *empty_json = "[]";
        result->data = strdup(empty_json);
        result->data_size = strlen(empty_json);
        result->content_type = strdup("application/json");
        PQclear(res);
        return result;
    }
    
    // Allocate a buffer for the JSON result
    // Initial size, will be expanded if needed
    size_t buffer_size = 1024;
    char *buffer = (char *)malloc(buffer_size);
    if (!buffer) {
        PQclear(res);
        s3_result_set_error(result, S3_ERROR_MEMORY, "Failed to allocate memory");
        return result;
    }
    
    // Start with opening bracket
    strcpy(buffer, "[");
    size_t buffer_pos = 1;
    
    for (int i = 0; i < rows; i++) {
        const char *path = PQgetvalue(res, i, 0);
        const char *size = PQgetvalue(res, i, 1);
        const char *lastmod = PQgetvalue(res, i, 2);
        
        // Format JSON for this object
        char obj_json[512];
        snprintf(obj_json, sizeof(obj_json), 
                "%s{\"Key\":\"%s\",\"Size\":%s,\"LastModified\":\"%s\"}",
                (i > 0) ? "," : "", path, size, lastmod);
        
        size_t obj_len = strlen(obj_json);
        
        // Check if we need to expand the buffer
        if (buffer_pos + obj_len + 2 > buffer_size) {
            buffer_size *= 2;
            char *new_buffer = (char *)realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                PQclear(res);
                s3_result_set_error(result, S3_ERROR_MEMORY, "Failed to allocate memory");
                return result;
            }
            buffer = new_buffer;
        }
        
        // Append this object to the JSON
        strcpy(buffer + buffer_pos, obj_json);
        buffer_pos += obj_len;
    }
    
    // Close the JSON array
    strcpy(buffer + buffer_pos, "]");
    buffer_pos += 1;
    
    result->data = buffer;
    result->data_size = buffer_pos;
    result->content_type = strdup("application/json");
    
    PQclear(res);
    return result;
}

/**
 * Get object from bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with object data
 */
S3Result* s3_api_get_object(PGconn *conn, const char *bucket, const char *key) {
    S3Result *result = s3_result_create();
    if (!result) {
        return NULL;
    }
    
    if (!conn) {
        s3_result_set_error(result, S3_ERROR_CONNECTION, "Invalid PostgreSQL connection");
        return result;
    }
    
    if (!bucket || !key) {
        s3_result_set_error(result, S3_ERROR_INVALID_INPUT, "Bucket name and key are required");
        return result;
    }
    
    // Check if the bucket is "public" (the only supported bucket)
    if (strcmp(bucket, "public") != 0) {
        s3_result_set_error(result, S3_ERROR_NOT_FOUND, "Bucket not found");
        return result;
    }
    
    // Ensure schema and tables exist
    if (ensure_s3_schema(conn) != 0) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to ensure schema");
        return result;
    }
    
    // Query object from database
    const char *query_template = 
        "SELECT content, content_type FROM s3.objects WHERE path = $1;";
    
    // Prepare parameters
    const char *params[1] = {key};
    
    // Execute parameterized query
    PGresult *res = PQexecParams(conn, query_template, 1, NULL, params, NULL, NULL, 0);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to query object");
        if (res) PQclear(res);
        return result;
    }
    
    if (PQntuples(res) == 0) {
        s3_result_set_error(result, S3_ERROR_NOT_FOUND, "Object not found");
        PQclear(res);
        return result;
    }
    
    // Get content and content type
    size_t content_size;
    unsigned char *content = PQunescapeBytea(
        (unsigned char *)PQgetvalue(res, 0, 0), 
        &content_size
    );
    
    if (!content) {
        s3_result_set_error(result, S3_ERROR_MEMORY, "Failed to unescape content");
        PQclear(res);
        return result;
    }
    
    const char *content_type = PQgetvalue(res, 0, 1);
    
    result->data = content;
    result->data_size = content_size;
    result->content_type = strdup(content_type);
    
    PQclear(res);
    return result;
}

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
                          const void *data, size_t size, const char *content_type) {
    S3Result *result = s3_result_create();
    if (!result) {
        return NULL;
    }
    
    if (!conn) {
        s3_result_set_error(result, S3_ERROR_CONNECTION, "Invalid PostgreSQL connection");
        return result;
    }
    
    if (!bucket || !key || !data || size == 0) {
        s3_result_set_error(result, S3_ERROR_INVALID_INPUT, "Bucket name, key, and data are required");
        return result;
    }
    
    // Default content type if not provided
    if (!content_type) {
        content_type = "application/octet-stream";
    }
    
    // Check if the bucket is "public" (the only supported bucket)
    if (strcmp(bucket, "public") != 0) {
        s3_result_set_error(result, S3_ERROR_NOT_FOUND, "Bucket not found");
        return result;
    }
    
    // Ensure schema and tables exist
    if (ensure_s3_schema(conn) != 0) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to ensure schema");
        return result;
    }
    
    // Escape binary data for SQL
    size_t escaped_size;
    unsigned char *escaped_data = PQescapeBytea(data, size, &escaped_size);
    if (!escaped_data) {
        s3_result_set_error(result, S3_ERROR_MEMORY, "Failed to escape data");
        return result;
    }
    
    // Prepare query parameters
    const char *params[4];
    params[0] = key;
    params[1] = (const char *)escaped_data;
    params[2] = content_type;
    
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%zu", size);
    params[3] = size_str;
    
    // Insert or update object
    const char *query = 
        "INSERT INTO s3.objects (path, content, content_type, size, last_modified) "
        "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP) "
        "ON CONFLICT (path) DO UPDATE "
        "SET content = $2, content_type = $3, size = $4, last_modified = CURRENT_TIMESTAMP "
        "RETURNING to_char(last_modified, 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') as lastmod;";
    
    // Execute parameterized query
    PGresult *res = PQexecParams(conn, query, 4, NULL, params, NULL, NULL, 0);
    PQfreemem(escaped_data);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, 
                            PQresultErrorMessage(res) ? PQresultErrorMessage(res) : "Failed to store object");
        if (res) PQclear(res);
        return result;
    }
    
    // Get the ETag (we'll use the last-modified timestamp for now)
    const char *lastmod = PQgetvalue(res, 0, 0);
    
    // Calculate MD5 hash as ETag (simplified)
    unsigned long hash = 5381;
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }
    
    char etag[32];
    snprintf(etag, sizeof(etag), "\"%08lx\"", hash);
    
    char json_response[256];
    snprintf(json_response, sizeof(json_response), 
            "{\"ETag\":\"%s\",\"LastModified\":\"%s\"}", 
            etag, lastmod);
    
    result->data = strdup(json_response);
    result->data_size = strlen(json_response);
    result->content_type = strdup("application/json");
    
    PQclear(res);
    return result;
}

/**
 * Delete object from bucket
 * 
 * @param conn PostgreSQL connection
 * @param bucket bucket name
 * @param key object key
 * @return S3Result with status
 */
S3Result* s3_api_delete_object(PGconn *conn, const char *bucket, const char *key) {
    S3Result *result = s3_result_create();
    if (!result) {
        return NULL;
    }
    
    if (!conn) {
        s3_result_set_error(result, S3_ERROR_CONNECTION, "Invalid PostgreSQL connection");
        return result;
    }
    
    if (!bucket || !key) {
        s3_result_set_error(result, S3_ERROR_INVALID_INPUT, "Bucket name and key are required");
        return result;
    }
    
    // Check if the bucket is "public" (the only supported bucket)
    if (strcmp(bucket, "public") != 0) {
        s3_result_set_error(result, S3_ERROR_NOT_FOUND, "Bucket not found");
        return result;
    }
    
    // Delete object from database
    const char *query_template = 
        "DELETE FROM s3.objects WHERE path = $1 RETURNING 1;";
    
    // Prepare parameters
    const char *params[1] = {key};
    
    // Execute parameterized query
    PGresult *res = PQexecParams(conn, query_template, 1, NULL, params, NULL, NULL, 0);
    if (!res) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, "Failed to delete object");
        return result;
    }
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        s3_result_set_error(result, S3_ERROR_EXECUTION, PQresultErrorMessage(res));
        PQclear(res);
        return result;
    }
    
    if (PQntuples(res) == 0) {
        // Object didn't exist, but we'll still return success per S3 behavior
        PQclear(res);
    } else {
        PQclear(res);
    }
    
    // Return empty JSON object per S3 API
    result->data = strdup("{}");
    result->data_size = 2;
    result->content_type = strdup("application/json");
    
    return result;
} 