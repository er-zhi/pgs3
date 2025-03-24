#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include "../pg/pg_client.h"

// URL paths for S3 API
#define S3_PATH_LIST_BUCKETS "/"
#define S3_PATH_LIST_OBJECTS "/public"
#define S3_PATH_OBJECT_PREFIX "/public/"

// Context for PUT request
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    char *content_type;
    const char *url;
    const char *method;
} RequestContext;

// Request handler structure
typedef struct {
    char *url;
    int (*handler)(HttpServer *, struct MHD_Connection *, const char *, const char *, size_t *);
} RequestHandler;

// Forward declarations
static int handle_list_buckets(HttpServer *server, struct MHD_Connection *connection, 
                               const char *url, const char *upload_data, size_t *upload_data_size);
static int handle_list_objects(HttpServer *server, struct MHD_Connection *connection, 
                               const char *url, const char *upload_data, size_t *upload_data_size);
static int handle_get_object(HttpServer *server, struct MHD_Connection *connection, 
                             const char *url, const char *upload_data, size_t *upload_data_size);
static int handle_put_object(HttpServer *server, struct MHD_Connection *connection, 
                             RequestContext *ctx, const char *upload_data, size_t *upload_data_size);
static int handle_delete_object(HttpServer *server, struct MHD_Connection *connection, 
                                const char *url, const char *upload_data, size_t *upload_data_size);

// Handle PUT data
static enum MHD_Result
put_data_handler(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
    RequestContext *ctx = (RequestContext *)cls;
    
    if (strcasecmp(key, "Content-Type") == 0) {
        ctx->content_type = strdup(value);
    }
    
    return MHD_YES;
}

// Clean up request data
static void
request_completed_callback(void *cls, struct MHD_Connection *connection,
                           void **con_cls, enum MHD_RequestTerminationCode toe)
{
    RequestContext *ctx = *con_cls;
    
    if (ctx) {
        if (ctx->data)
            free(ctx->data);
        if (ctx->content_type)
            free(ctx->content_type);
        free(ctx);
        *con_cls = NULL;
    }
}

// Main request handler callback
static enum MHD_Result
request_handler(void *cls, struct MHD_Connection *connection,
                const char *url, const char *method,
                const char *version, const char *upload_data,
                size_t *upload_data_size, void **con_cls)
{
    HttpServer *server = (HttpServer *)cls;
    
    if (*con_cls == NULL) {
        // First call for this request
        RequestContext *ctx = malloc(sizeof(RequestContext));
        if (!ctx) return MHD_NO;
        
        ctx->data = NULL;
        ctx->size = 0;
        ctx->capacity = 0;
        ctx->content_type = NULL;
        ctx->url = url;
        ctx->method = method;
        
        // For PUT requests, get the content type
        if (strcmp(method, "PUT") == 0) {
            MHD_get_connection_values(connection, MHD_HEADER_KIND, &put_data_handler, ctx);
            
            // Set default content type if not provided
            if (!ctx->content_type) {
                ctx->content_type = strdup("application/octet-stream");
            }
        }
        
        *con_cls = ctx;
        return MHD_YES;
    }
    
    RequestContext *ctx = *con_cls;
    
    printf("Processing request: %s %s\n", method, url);
    
    // Handle PUT data upload
    if (strcmp(method, "PUT") == 0 && *upload_data_size > 0) {
        // Allocate or expand buffer as needed
        if (ctx->capacity < ctx->size + *upload_data_size) {
            size_t new_capacity = ctx->capacity == 0 ? 
                *upload_data_size * 2 : (ctx->size + *upload_data_size) * 2;
            
            char *new_data = realloc(ctx->data, new_capacity);
            if (!new_data) {
                return MHD_NO;
            }
            
            ctx->data = new_data;
            ctx->capacity = new_capacity;
        }
        
        // Copy the data
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        
        // Mark this chunk as processed
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // Process the actual request based on method and URL
    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, S3_PATH_LIST_BUCKETS) == 0) {
            // List buckets
            return handle_list_buckets(server, connection, url, upload_data, upload_data_size);
        } else if (strcmp(url, S3_PATH_LIST_OBJECTS) == 0) {
            // List objects in bucket
            return handle_list_objects(server, connection, url, upload_data, upload_data_size);
        } else if (strncmp(url, S3_PATH_OBJECT_PREFIX, strlen(S3_PATH_OBJECT_PREFIX)) == 0) {
            // Get object
            return handle_get_object(server, connection, url, upload_data, upload_data_size);
        }
    } else if (strcmp(method, "PUT") == 0) {
        if (strncmp(url, S3_PATH_OBJECT_PREFIX, strlen(S3_PATH_OBJECT_PREFIX)) == 0) {
            // Put object (only process when we have all data)
            return handle_put_object(server, connection, ctx, upload_data, upload_data_size);
        }
    } else if (strcmp(method, "DELETE") == 0) {
        if (strncmp(url, S3_PATH_OBJECT_PREFIX, strlen(S3_PATH_OBJECT_PREFIX)) == 0) {
            // Delete object
            return handle_delete_object(server, connection, url, upload_data, upload_data_size);
        }
    }
    
    // Method not allowed or path not found
    const char *not_found = "Not Found";
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(not_found), (void *)not_found, MHD_RESPMEM_PERSISTENT);
    
    int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle list buckets (GET /)
static int handle_list_buckets(HttpServer *server, struct MHD_Connection *connection, 
                               const char *url, const char *upload_data, size_t *upload_data_size)
{
    S3Result *result = pg_client_list_buckets(server->pg_client);
    if (!result || result->status != S3_SUCCESS) {
        const char *error = "Internal Server Error";
        if (result && result->error_message) {
            error = result->error_message;
        }
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        
        if (result) s3_result_free(result);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        result->data_size, result->data, MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(response, "Content-Type", result->content_type);
    
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    s3_result_free(result);
    return ret;
}

// Handle list objects (GET /public)
static int handle_list_objects(HttpServer *server, struct MHD_Connection *connection, 
                               const char *url, const char *upload_data, size_t *upload_data_size)
{
    // Get prefix parameter if present
    const char *prefix = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "prefix");
    
    S3Result *result = pg_client_list_objects(server->pg_client, "public");
    if (!result || result->status != S3_SUCCESS) {
        const char *error = "Internal Server Error";
        if (result && result->error_message) {
            error = result->error_message;
        }
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        
        if (result) s3_result_free(result);
        return ret;
    }
    
    // Apply prefix filter if needed
    if (prefix && *prefix && result->data) {
        // Simple JSON parsing to filter by prefix
        // In a real implementation, use a JSON library
        char *json = (char*)result->data;
        char *filtered = malloc(result->data_size);
        if (!filtered) {
            s3_result_free(result);
            const char *error = "Memory allocation failed";
            struct MHD_Response *response = MHD_create_response_from_buffer(
                strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
            
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }
        
        // Start with opening bracket
        strcpy(filtered, "[");
        size_t pos = 1;
        
        // Find objects in JSON array
        char *obj_start = strchr(json, '{');
        int count = 0;
        
        while (obj_start) {
            // Find end of this object
            char *obj_end = strchr(obj_start, '}');
            if (!obj_end) break;
            
            // Extract key from this object
            char *key_start = strstr(obj_start, "\"Key\":\"");
            if (!key_start || key_start > obj_end) {
                // Move to next object
                obj_start = strchr(obj_end, '{');
                continue;
            }
            
            key_start += 7; // Skip "Key":"
            char *key_end = strchr(key_start, '\"');
            if (!key_end || key_end > obj_end) {
                // Move to next object
                obj_start = strchr(obj_end, '{');
                continue;
            }
            
            // Check if key starts with prefix
            size_t prefix_len = strlen(prefix);
            size_t key_len = key_end - key_start;
            if (key_len >= prefix_len && strncmp(key_start, prefix, prefix_len) == 0) {
                // Include this object in filtered result
                if (count > 0) {
                    filtered[pos++] = ',';
                }
                
                // Copy the object
                size_t obj_len = obj_end - obj_start + 1;
                strncpy(filtered + pos, obj_start, obj_len);
                pos += obj_len;
                count++;
            }
            
            // Move to next object
            obj_start = strchr(obj_end, '{');
        }
        
        // Close the array
        filtered[pos++] = ']';
        filtered[pos] = '\0';
        
        // Create response from filtered data
        struct MHD_Response *response = MHD_create_response_from_buffer(
            pos, filtered, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(response, "Content-Type", result->content_type);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        
        s3_result_free(result);
        return ret;
    }
    
    // No prefix filter, return all objects
    struct MHD_Response *response = MHD_create_response_from_buffer(
        result->data_size, result->data, MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(response, "Content-Type", result->content_type);
    
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    s3_result_free(result);
    return ret;
}

// Handle get object (GET /public/<key>)
static int handle_get_object(HttpServer *server, struct MHD_Connection *connection, 
                             const char *url, const char *upload_data, size_t *upload_data_size)
{
    // Extract key from URL (skip "/public/")
    const char *key = url + strlen(S3_PATH_OBJECT_PREFIX);
    
    S3Result *result = pg_client_get_object(server->pg_client, "public", key);
    if (!result) {
        const char *error = "Internal Server Error";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    if (result->status != S3_SUCCESS) {
        int status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        
        // Map S3 error to HTTP status
        if (result->status == S3_ERROR_NOT_FOUND) {
            status_code = MHD_HTTP_NOT_FOUND;
        } else if (result->status == S3_ERROR_PERMISSION) {
            status_code = MHD_HTTP_FORBIDDEN;
        }
        
        const char *error = result->error_message ? result->error_message : "Error";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, status_code, response);
        MHD_destroy_response(response);
        
        s3_result_free(result);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        result->data_size, result->data, MHD_RESPMEM_MUST_COPY);
    
    if (result->content_type) {
        MHD_add_response_header(response, "Content-Type", result->content_type);
    }
    
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    s3_result_free(result);
    return ret;
}

// Handle put object (PUT /public/<key>)
static int handle_put_object(HttpServer *server, struct MHD_Connection *connection, 
                             RequestContext *ctx, const char *upload_data, size_t *upload_data_size)
{
    // Extract key from URL (skip "/public/")
    const char *key = ctx->url + strlen(S3_PATH_OBJECT_PREFIX);
    
    // Put the object
    S3Result *result = pg_client_put_object(
        server->pg_client, "public", key, ctx->data, ctx->size, ctx->content_type);
    
    if (!result || result->status != S3_SUCCESS) {
        const char *error = "Internal Server Error";
        if (result && result->error_message) {
            error = result->error_message;
        }
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        
        if (result) s3_result_free(result);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        result->data_size, result->data, MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(response, "Content-Type", result->content_type);
    
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    s3_result_free(result);
    return ret;
}

// Handle delete object (DELETE /public/<key>)
static int handle_delete_object(HttpServer *server, struct MHD_Connection *connection, 
                                const char *url, const char *upload_data, size_t *upload_data_size)
{
    // Extract key from URL (skip "/public/")
    const char *key = url + strlen(S3_PATH_OBJECT_PREFIX);
    
    S3Result *result = pg_client_delete_object(server->pg_client, "public", key);
    if (!result || result->status != S3_SUCCESS) {
        const char *error = "Internal Server Error";
        if (result && result->error_message) {
            error = result->error_message;
        }
        
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        
        if (result) s3_result_free(result);
        return ret;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        result->data_size, result->data, MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(response, "Content-Type", result->content_type);
    
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    s3_result_free(result);
    return ret;
}

/**
 * Initialize HTTP server
 * 
 * @param port port to listen on
 * @param pg_conninfo PostgreSQL connection string
 * @return pointer to HttpServer structure or NULL if error
 */
HttpServer *http_server_init(int port, const char *pg_conninfo) {
    HttpServer *server = (HttpServer *)malloc(sizeof(HttpServer));
    if (!server) {
        return NULL;
    }
    
    server->port = port;
    server->daemon = NULL;
    
    // Initialize PostgreSQL client
    server->pg_client = pg_client_init(pg_conninfo);
    if (!server->pg_client) {
        free(server);
        return NULL;
    }
    
    return server;
}

/**
 * Run HTTP server (blocking call)
 * 
 * @param server pointer to HttpServer structure
 * @return 0 on success, -1 on error
 */
int http_server_run(HttpServer *server) {
    if (!server) {
        return -1;
    }
    
    // Start HTTP daemon
    server->daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
        server->port, NULL, NULL,
        &request_handler, server,
        MHD_OPTION_NOTIFY_COMPLETED, request_completed_callback, NULL,
        MHD_OPTION_END);
    
    if (!server->daemon) {
        return -1;
    }
    
    printf("HTTP server listening on port %d\n", server->port);
    
    // This is a blocking call - the server will run until stopped
    // In a real implementation, we would use signals to handle graceful shutdown
    while (1) {
        sleep(1);
    }
    
    return 0;
}

/**
 * Free HTTP server resources
 * 
 * @param server pointer to HttpServer structure
 */
void http_server_free(HttpServer *server) {
    if (!server) {
        return;
    }
    
    if (server->daemon) {
        MHD_stop_daemon(server->daemon);
    }
    
    if (server->pg_client) {
        pg_client_free(server->pg_client);
    }
    
    free(server);
} 