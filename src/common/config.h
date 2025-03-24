#ifndef CONFIG_H
#define CONFIG_H

// Default values
#define DEFAULT_HTTP_PORT 9000
#define DEFAULT_PG_CONNINFO "host=localhost user=postgres password=postgres dbname=postgres"

// Configuration structure
typedef struct {
    unsigned int http_port;
    char *pg_conninfo;
} Config;

// Functions for config management
Config *config_init(void);
void config_free(Config *config);
int config_parse_args(Config *config, int argc, char **argv);

#endif /* CONFIG_H */ 