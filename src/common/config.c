#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/**
 * Initialize configuration with default values
 * 
 * @return pointer to Config structure or NULL if error
 */
Config *config_init(void) {
    Config *config = (Config *)malloc(sizeof(Config));
    if (!config) {
        return NULL;
    }
    
    config->http_port = DEFAULT_HTTP_PORT;
    config->pg_conninfo = strdup(DEFAULT_PG_CONNINFO);
    
    if (!config->pg_conninfo) {
        free(config);
        return NULL;
    }
    
    return config;
}

/**
 * Free configuration resources
 * 
 * @param config pointer to Config structure
 */
void config_free(Config *config) {
    if (!config) {
        return;
    }
    
    if (config->pg_conninfo) {
        free(config->pg_conninfo);
    }
    
    free(config);
}

/**
 * Print usage help
 * 
 * @param program program name
 */
static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("Options:\n");
    printf("  -p, --port PORT       HTTP port (default: %d)\n", DEFAULT_HTTP_PORT);
    printf("  -d, --db CONNINFO     PostgreSQL connection string (default: %s)\n", DEFAULT_PG_CONNINFO);
    printf("  -h, --help            Display this help message\n");
}

/**
 * Parse command line arguments
 * 
 * @param config pointer to Config structure
 * @param argc argument count
 * @param argv argument values
 * @return 0 on success, -1 on error
 */
int config_parse_args(Config *config, int argc, char **argv) {
    if (!config) {
        return -1;
    }
    
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"db", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:d:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                config->http_port = atoi(optarg);
                if (config->http_port <= 0 || config->http_port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return -1;
                }
                break;
                
            case 'd':
                if (config->pg_conninfo) {
                    free(config->pg_conninfo);
                }
                config->pg_conninfo = strdup(optarg);
                if (!config->pg_conninfo) {
                    return -1;
                }
                break;
                
            case 'h':
                print_usage(argv[0]);
                return 1;
                
            case '?':
                // getopt_long already printed an error message
                return -1;
                
            default:
                fprintf(stderr, "Unexpected option: %c\n", c);
                return -1;
        }
    }
    
    return 0;
} 