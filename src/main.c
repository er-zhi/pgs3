#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pg/pg_client.h"
#include "http/http_server.h"

void print_help() {
    printf("PostgreSQL S3 CLI\n");
    printf("Usage: pgs3 <command> [options]\n\n");
    printf("Commands:\n");
    printf("  ls [prefix]             List objects in the public bucket, optionally with prefix\n");
    printf("  get <key>               Get object from public bucket\n");
    printf("  put <key>               Put object from stdin into public bucket\n");
    printf("  delete <key>            Delete object from public bucket\n");
    printf("  serve [port]            Start HTTP server (default port: 9000)\n");
    printf("\n");
    printf("Environment variables:\n");
    printf("  PGHOST                  PostgreSQL host (default: localhost)\n");
    printf("  PGPORT                  PostgreSQL port (default: 5432)\n");
    printf("  PGDATABASE              PostgreSQL database name (default: postgres)\n");
    printf("  PGUSER                  PostgreSQL user (default: postgres)\n");
    printf("  PGPASSWORD              PostgreSQL password (default: postgres)\n");
    printf("  PGCONNSTRING            Full PostgreSQL connection string (overrides other variables)\n");
}

int main(int argc, char *argv[]) {
    // Build connection string from environment variables or use defaults
    char conninfo[256] = {0};
    
    // Check if a full connection string is provided
    const char *pgconnstring = getenv("PGCONNSTRING");
    if (pgconnstring) {
        strncpy(conninfo, pgconnstring, sizeof(conninfo) - 1);
    } else {
        // Build connection string from individual parameters
        const char *pghost = getenv("PGHOST");
        const char *pgport = getenv("PGPORT");
        const char *pgdatabase = getenv("PGDATABASE");
        const char *pguser = getenv("PGUSER");
        const char *pgpassword = getenv("PGPASSWORD");
        
        // Use defaults for missing values
        snprintf(conninfo, sizeof(conninfo),
                "host=%s port=%s dbname=%s user=%s password=%s",
                pghost ? pghost : "localhost",
                pgport ? pgport : "5432",
                pgdatabase ? pgdatabase : "postgres",
                pguser ? pguser : "postgres",
                pgpassword ? pgpassword : "postgres");
    }
    
    // Check for command
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    // Check for serve command
    if (strcmp(argv[1], "serve") == 0) {
        // Handle serve command
        int port = 9000; // Default port
        if (argc > 2) {
            port = atoi(argv[2]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port number. Using default port 9000.\n");
                port = 9000;
            }
        }
        
        // Start HTTP server
        HttpServer *server = http_server_init(port, conninfo);
        if (!server) {
            fprintf(stderr, "Failed to initialize HTTP server\n");
            return 1;
        }
        
        printf("Starting S3 API server on port %d\n", port);
        printf("Serving bucket 'public'\n");
        printf("Press Ctrl+C to stop\n");
        
        // Run HTTP server (blocking call)
        int result = http_server_run(server);
        
        // Cleanup (this will only be reached if server stops)
        http_server_free(server);
        return result;
    }
    
    // Initialize PostgreSQL client
    PgClient *client = pg_client_init(conninfo);
    if (!client) {
        fprintf(stderr, "Failed to connect to PostgreSQL\n");
        return 1;
    }

    int result = 0;

    // Process command
    if (strcmp(argv[1], "ls") == 0) {
        // Handle ls command
        // Можно указать префикс как аргумент
        if (argc > 2) {
            printf("Listing objects with prefix: %s\n", argv[2]);
            // В будущей версии здесь будет фильтрация по префиксу
        }
        
        S3Result *s3_result = pg_client_list_objects(client, "public");
        if (s3_result && s3_result->status == S3_SUCCESS) {
            printf("%s\n", (char*)s3_result->data);
        } else {
            fprintf(stderr, "Failed to list objects\n");
            if (s3_result && s3_result->error_message) {
                fprintf(stderr, "%s\n", s3_result->error_message);
            }
            result = 1;
        }
        if (s3_result) {
            s3_result_free(s3_result);
        }
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: pgs3 get <key>\n");
            pg_client_free(client);
            return 1;
        }
        
        // Always use the 'public' bucket
        S3Result *result = pg_client_get_object(client, "public", argv[2]);
        if (result) {
            if (result->status == S3_SUCCESS) {
                // Write content to stdout
                fwrite(result->data, 1, result->data_size, stdout);
            } else {
                fprintf(stderr, "Error: %s\n", result->error_message ? result->error_message : "Unknown error");
            }
            s3_result_free(result);
        } else {
            fprintf(stderr, "Failed to execute command\n");
            pg_client_free(client);
            return 1;
        }
    } else if (strcmp(argv[1], "put") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: pgs3 put <key>\n");
            pg_client_free(client);
            return 1;
        }
        
        // Read data from stdin
        size_t size = 0;
        size_t capacity = 1024;
        char *data = malloc(capacity);
        if (!data) {
            fprintf(stderr, "Failed to allocate memory\n");
            pg_client_free(client);
            return 1;
        }
        
        int c;
        while ((c = getchar()) != EOF) {
            if (size >= capacity) {
                capacity *= 2;
                char *new_data = realloc(data, capacity);
                if (!new_data) {
                    fprintf(stderr, "Failed to allocate memory\n");
                    free(data);
                    pg_client_free(client);
                    return 1;
                }
                data = new_data;
            }
            data[size++] = c;
        }
        
        // Try to determine content type from key
        const char *content_type = "application/octet-stream";
        char *ext = strrchr(argv[2], '.');
        if (ext) {
            ext++; // Skip the dot
            if (strcasecmp(ext, "txt") == 0) {
                content_type = "text/plain";
            } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
                content_type = "text/html";
            } else if (strcasecmp(ext, "css") == 0) {
                content_type = "text/css";
            } else if (strcasecmp(ext, "js") == 0) {
                content_type = "application/javascript";
            } else if (strcasecmp(ext, "json") == 0) {
                content_type = "application/json";
            } else if (strcasecmp(ext, "xml") == 0) {
                content_type = "application/xml";
            } else if (strcasecmp(ext, "png") == 0) {
                content_type = "image/png";
            } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
                content_type = "image/jpeg";
            } else if (strcasecmp(ext, "gif") == 0) {
                content_type = "image/gif";
            } else if (strcasecmp(ext, "pdf") == 0) {
                content_type = "application/pdf";
            }
        }
        
        // Always use the 'public' bucket
        S3Result *result = pg_client_put_object(client, "public", argv[2], data, size, content_type);
        free(data);
        
        if (result) {
            if (result->status == S3_SUCCESS) {
                printf("%.*s\n", (int)result->data_size, (char*)result->data);
            } else {
                fprintf(stderr, "Error: %s\n", result->error_message ? result->error_message : "Unknown error");
            }
            s3_result_free(result);
        } else {
            fprintf(stderr, "Failed to execute command\n");
            pg_client_free(client);
            return 1;
        }
    } else if (strcmp(argv[1], "delete") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: pgs3 delete <key>\n");
            pg_client_free(client);
            return 1;
        }
        
        // Always use the 'public' bucket
        S3Result *result = pg_client_delete_object(client, "public", argv[2]);
        if (result) {
            if (result->status == S3_SUCCESS) {
                printf("Object deleted successfully\n");
            } else {
                fprintf(stderr, "Error: %s\n", result->error_message ? result->error_message : "Unknown error");
            }
            s3_result_free(result);
        } else {
            fprintf(stderr, "Failed to execute command\n");
            pg_client_free(client);
            return 1;
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_help();
        pg_client_free(client);
        return 1;
    }
    
    pg_client_free(client);
    return result;
} 