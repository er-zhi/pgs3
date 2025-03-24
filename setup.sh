#!/bin/bash

# PGS3 - PostgreSQL S3 API Client setup script
# This script helps to set up the development environment for pgs3

echo "Setting up pgs3 development environment..."

# Check for required commands
check_command() {
    if ! command -v $1 &> /dev/null; then
        echo "Error: $1 is not installed. Please install it first."
        return 1
    fi
    return 0
}

# Check for required packages
check_command gcc || exit 1
check_command make || exit 1

# Check for PostgreSQL client libraries
PG_CONFIG=""
if command -v pg_config &> /dev/null; then
    PG_CONFIG="pg_config"
    echo "Found pg_config at $(which pg_config)"
else
    echo "Warning: pg_config not found. This may cause issues with finding PostgreSQL includes."
    echo "You may need to install PostgreSQL development package."
    
    # Suggest installation commands based on OS
    if [ -f /etc/debian_version ]; then
        echo "Try: sudo apt-get install libpq-dev"
    elif [ -f /etc/redhat-release ]; then
        echo "Try: sudo yum install postgresql-devel"
    elif [ -f /etc/arch-release ]; then
        echo "Try: sudo pacman -S postgresql-libs"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "Try: brew install libpq"
        echo "Then: brew link --force libpq"
    fi
fi

# Check for libmicrohttpd
MICROHTTPD_FOUND=0
if pkg-config --exists libmicrohttpd; then
    echo "Found libmicrohttpd $(pkg-config --modversion libmicrohttpd)"
    MICROHTTPD_FOUND=1
else
    # Check common paths
    if [[ -f /usr/include/microhttpd.h ]] || [[ -f /usr/local/include/microhttpd.h ]]; then
        echo "Found libmicrohttpd in system paths"
        MICROHTTPD_FOUND=1
    elif [[ "$(uname)" == "Darwin" ]] && [[ -f /usr/local/opt/libmicrohttpd/include/microhttpd.h ]]; then
        echo "Found libmicrohttpd in Homebrew path"
        MICROHTTPD_FOUND=1
    else
        echo "Warning: libmicrohttpd not found. You won't be able to use the HTTP server."
        echo "To enable HTTP server support, install libmicrohttpd:"
        
        # Suggest installation commands based on OS
        if [ -f /etc/debian_version ]; then
            echo "Try: sudo apt-get install libmicrohttpd-dev"
        elif [ -f /etc/redhat-release ]; then
            echo "Try: sudo yum install libmicrohttpd-devel"
        elif [ -f /etc/arch-release ]; then
            echo "Try: sudo pacman -S libmicrohttpd"
        elif [[ "$(uname)" == "Darwin" ]]; then
            echo "Try: brew install libmicrohttpd"
        fi
    fi
fi

# Create directories
echo "Creating build directories..."
mkdir -p obj/pg obj/http bin

# Check if PostgreSQL headers are found
if [ -n "$PG_CONFIG" ]; then
    PG_INCLUDEDIR=$($PG_CONFIG --includedir)
    if [ ! -f "$PG_INCLUDEDIR/libpq-fe.h" ]; then
        echo "Warning: libpq-fe.h not found in $PG_INCLUDEDIR"
    else
        echo "Found libpq-fe.h in $PG_INCLUDEDIR"
    fi
fi

# Build the project
echo "Building pgs3..."
make

if [ $? -eq 0 ]; then
    echo "Build successful! The binary is available at bin/pgs3"
    echo
    echo "To install system-wide, run: sudo make install"
    echo
    echo -e "\e[1mSetup complete!\e[0m"
    echo "You can now use pgs3. Some example commands:"
    echo "  bin/pgs3 ls"
    echo "  echo 'test content' | bin/pgs3 put test.txt"
    echo "  bin/pgs3 get test.txt"
    echo "  bin/pgs3 serve 9000"
    if [ $MICROHTTPD_FOUND -eq 1 ]; then
        echo "To start the S3 HTTP server:"
        echo "  bin/pgs3 serve 9000"
    fi
else
    echo "Build failed."
    echo
    echo "If you see a 'libpq-fe.h not found' error, you may need to update your CFLAGS to include PostgreSQL headers."
    echo "Try adding the include path to the Makefile CFLAGS line:"
    echo
    if [ -n "$PG_CONFIG" ]; then
        echo "CFLAGS = -Wall -Werror -g -I$PG_INCLUDEDIR"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "CFLAGS = -Wall -Werror -g -I/usr/local/opt/libpq/include"
        echo
        echo "You may also need to update LDFLAGS:"
        echo "LDFLAGS = -L/usr/local/opt/libpq/lib -lpq"
    else
        echo "CFLAGS = -Wall -Werror -g -I/usr/include/postgresql"
    fi
    echo
    if [ $MICROHTTPD_FOUND -eq 0 ]; then
        echo "If you see a 'microhttpd.h not found' error, you need to install libmicrohttpd development package."
    fi
fi

exit 0 