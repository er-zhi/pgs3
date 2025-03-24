#!/bin/sh
set -e

# Start PostgreSQL in background
echo "Starting PostgreSQL..."
su - postgres -c "postgres -D $PGDATA" &

# Wait for PostgreSQL to start
echo "Waiting for PostgreSQL to start..."
sleep 5

# Create test database if it doesn't exist
echo "Checking PostgreSQL connection..."
if ! su - postgres -c "psql -c '\l'" | grep -q postgres; then
    echo "Creating postgres database..."
    su - postgres -c "createdb postgres"
fi

# Initialize environment variables
export PGHOST=localhost
export PGUSER=postgres
export PGPASSWORD=postgres
export PGDATABASE=postgres
export PGCONNSTRING="host=localhost dbname=postgres user=postgres password=postgres"

# Set S3 port (default: 9000)
PORT=${AWS_S3_PORT:-9000}
echo "Using S3 API port: $PORT"

# Start S3 API server
echo "Starting S3 API server on port $PORT..."
cd /app && ./bin/pgs3 serve $PORT 