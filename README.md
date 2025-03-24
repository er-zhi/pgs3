# PGS3 - PostgreSQL S3 API Client (v0.0.1)

A command-line interface for interacting with PostgreSQL's S3 API functionality. This implementation stores files in the PostgreSQL database and provides an S3-compatible interface.

> ⚠️ **Note**: This is version 0.0.1 with basic functionality only. The project is currently under active development.

## System Requirements

- PostgreSQL 17.4 (development and testing done with this version)
- Currently supports **arm64** architecture (tested on Apple Silicon M-series chips)
- C compiler (gcc recommended)
- libpq (PostgreSQL client libraries)
- libmicrohttpd (for HTTP server support)
- GNU Make

## Building

For an easier setup, use the provided setup script:

```bash
./setup.sh
```

This script will check for dependencies, help with any installation issues, and build the project.

Or you can build manually:

```bash
make
```

This will create the `pgs3` executable in the `bin` directory.

## Docker Installation

The easiest way to run the project is using Docker:

```bash
# Clone the repository
git clone https://github.com/yourusername/pgs3.git
cd pgs3

# Build and start the containers
docker-compose up -d
```

This will start a PostgreSQL server and the S3 API on port 9000.

You can customize the S3 API port by setting the `AWS_S3_PORT` environment variable:

```bash
AWS_S3_PORT=8080 docker-compose up -d
```

## Manual Installation

To install the executable to your system:

```bash
sudo make install
```

Or simply copy the `bin/pgs3` executable to a directory in your PATH:

```bash
sudo cp bin/pgs3 /usr/local/bin/
```

## Quick Start

The extension automatically creates the necessary schema and tables on first use. Just set your database connection and start using the commands:

```bash
# Set PostgreSQL connection info
export PGUSER=your_username
export PGPASSWORD=your_password
export PGDATABASE=your_database

# List objects
pgs3 ls

# Store a file
echo "Hello, world!" | pgs3 put hello.txt

# Retrieve a file
pgs3 get hello.txt > hello.txt

# Start HTTP server
pgs3 serve 9000
```

## Usage

```
PostgreSQL S3 CLI
Usage: pgs3 <command> [options]

Commands:
  ls [prefix]             List objects in the public bucket, optionally with prefix
  get <key>               Get object from public bucket
  put <key>               Put object from stdin into public bucket
  delete <key>            Delete object from public bucket
  serve [port]            Start HTTP server (default port: 9000)

Environment variables:
  PGHOST                  PostgreSQL host (default: localhost)
  PGPORT                  PostgreSQL port (default: 5432)
  PGDATABASE              PostgreSQL database name (default: postgres)
  PGUSER                  PostgreSQL user (default: postgres)
  PGPASSWORD              PostgreSQL password (default: postgres)
  PGCONNSTRING            Full PostgreSQL connection string (overrides other variables)
  AWS_S3_PORT             Port for S3 HTTP server (default: 9000)
```

### CLI Examples

List all objects in the public bucket:
```bash
pgs3 ls
```

List objects with a prefix:
```bash
pgs3 ls folder/
```

Get an object:
```bash
pgs3 get hello.txt
```

Put an object:
```bash
echo "Hello, S3!" | pgs3 put hello.txt
```

Delete an object:
```bash
pgs3 delete hello.txt
```

### HTTP Server

The HTTP server provides an S3-compatible API for using the system with standard S3 clients. To start the server:

```bash
pgs3 serve 9000
```

This will start a server on port 9000 that serves the 'public' bucket.

#### HTTP API Endpoints

- `GET /` - List all buckets (will only contain the 'public' bucket)
- `GET /public` - List all objects in the public bucket
- `GET /public?prefix=folder/` - List objects with prefix
- `GET /public/path/to/file.txt` - Get an object
- `PUT /public/path/to/file.txt` - Upload an object
- `DELETE /public/path/to/file.txt` - Delete an object

Example using curl:

```bash
# List objects
curl http://localhost:9000/public

# Upload file
curl -T myfile.txt http://localhost:9000/public/myfile.txt

# Download file
curl http://localhost:9000/public/myfile.txt > myfile.txt

# Delete file
curl -X DELETE http://localhost:9000/public/myfile.txt
```

## Configuration

You can configure the PostgreSQL connection using standard PostgreSQL environment variables:

```bash
# Basic connection parameters
export PGHOST=localhost
export PGPORT=5432
export PGDATABASE=postgres
export PGUSER=postgres
export PGPASSWORD=postgres

# Or use a full connection string
export PGCONNSTRING="host=localhost port=5432 dbname=postgres user=postgres password=postgres"

# S3 API port configuration
export AWS_S3_PORT=9000
```

## Testing

To run the test suite:

```bash
# Make sure you have PostgreSQL running
./tests/run_tests.sh
```

The tests cover basic functionality of both the CLI and HTTP server interfaces.

## Implementation Details

This implementation:
- Works with a single bucket named 'public'
- Follows AWS S3 API conventions for compatibility
- Stores file paths like a filesystem
- Stores files directly in the PostgreSQL database
- Creates the necessary schema and tables automatically
- Handles content types based on file extensions
- Provides both CLI and HTTP server interfaces

## Database Schema

The implementation creates a schema `s3` with the following table:

```sql
CREATE TABLE s3.objects (
   path TEXT PRIMARY KEY,
   content BYTEA NOT NULL,
   content_type TEXT NOT NULL,
   size BIGINT NOT NULL,
   last_modified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

## Development

The code is organized as follows:
- `src/main.c`: Main entry point and command processing
- `src/pg/pg_client.c`: PostgreSQL client wrapper
- `src/pg/s3_api.c`: S3 API implementation
- `src/http/http_server.c`: HTTP server implementation

To add new functionality, extend the S3 API in `src/pg/s3_api.c` and update the command handling in `src/main.c` or the HTTP handling in `src/http/http_server.c` as needed.

## Future Plans

- Support for more S3 API operations
- Multiple bucket support
- Authentication and access control
- Automatic content type detection
- Optimizations for large files
- Support for more architectures and platforms

## License

PGS3 is released under the [PostgreSQL License](LICENSE), a liberal open source license similar to BSD or MIT licenses. This license was specifically chosen to ensure compatibility with the PostgreSQL core project, allowing for potential integration as a core extension in the future.

If you're interested in contributing to this project with the aim of making it part of the PostgreSQL core, please ensure your contributions follow the [PostgreSQL Coding Standards](https://www.postgresql.org/docs/current/source.html) and the project's licensing terms.
