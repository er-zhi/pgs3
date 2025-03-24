#!/bin/bash
set -e

# Ensure we're in the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

# Set default environment variables for testing
export PGHOST=${PGHOST:-localhost}
export PGPORT=${PGPORT:-5432}
export PGDATABASE=${PGDATABASE:-postgres}
export PGUSER=${PGUSER:-postgres}
export PGPASSWORD=${PGPASSWORD:-postgres}
export AWS_S3_PORT=${AWS_S3_PORT:-9000}

# Ensure we have a built binary
if [ ! -f "bin/pgs3" ]; then
    echo "Building pgs3..."
    make
fi

echo "Running CLI tests..."

# Create a test file
TEST_CONTENT="This is a test file - $(date)"
TEST_FILE="test-$(date +%s).txt"
echo "$TEST_CONTENT" > "/tmp/$TEST_FILE"

# Test put command
echo -n "Testing put command: "
cat "/tmp/$TEST_FILE" | bin/pgs3 put "$TEST_FILE" > /dev/null && echo "OK" || { echo "FAILED"; exit 1; }

# Test ls command
echo -n "Testing ls command: "
bin/pgs3 ls | grep -q "$TEST_FILE" && echo "OK" || { echo "FAILED"; exit 1; }

# Test get command
echo -n "Testing get command: "
GET_CONTENT=$(bin/pgs3 get "$TEST_FILE")
[ "$GET_CONTENT" = "$TEST_CONTENT" ] && echo "OK" || { echo "FAILED"; exit 1; }

# Test delete command
echo -n "Testing delete command: "
bin/pgs3 delete "$TEST_FILE" > /dev/null && echo "OK" || { echo "FAILED"; exit 1; }

# Test HTTP server
echo "Running HTTP server tests..."

# Start HTTP server in background
bin/pgs3 serve $AWS_S3_PORT > /dev/null 2>&1 &
SERVER_PID=$!

# Wait for server to start
echo "Waiting for server to start..."
sleep 2

# Test list buckets
echo -n "Testing GET /: "
curl -s "http://localhost:$AWS_S3_PORT/" | grep -q "public" && echo "OK" || { echo "FAILED"; kill $SERVER_PID; exit 1; }

# Test put object
echo -n "Testing PUT /public/$TEST_FILE: "
curl -s -X PUT -T "/tmp/$TEST_FILE" "http://localhost:$AWS_S3_PORT/public/$TEST_FILE" > /dev/null && echo "OK" || { echo "FAILED"; kill $SERVER_PID; exit 1; }

# Test list objects
echo -n "Testing GET /public: "
curl -s "http://localhost:$AWS_S3_PORT/public" | grep -q "$TEST_FILE" && echo "OK" || { echo "FAILED"; kill $SERVER_PID; exit 1; }

# Test get object
echo -n "Testing GET /public/$TEST_FILE: "
HTTP_CONTENT=$(curl -s "http://localhost:$AWS_S3_PORT/public/$TEST_FILE")
[ "$HTTP_CONTENT" = "$TEST_CONTENT" ] && echo "OK" || { echo "FAILED"; kill $SERVER_PID; exit 1; }

# Test delete object
echo -n "Testing DELETE /public/$TEST_FILE: "
curl -s -X DELETE "http://localhost:$AWS_S3_PORT/public/$TEST_FILE" > /dev/null && echo "OK" || { echo "FAILED"; kill $SERVER_PID; exit 1; }

# Clean up
kill $SERVER_PID
rm -f "/tmp/$TEST_FILE"

echo "All tests passed!" 