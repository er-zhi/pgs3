FROM alpine:3.19

# Install required packages
RUN apk add --no-cache \
    postgresql-client \
    postgresql \
    postgresql-dev \
    gcc \
    make \
    musl-dev \
    libmicrohttpd \
    libmicrohttpd-dev

# Set up PostgreSQL
ENV PGDATA /var/lib/postgresql/data
RUN mkdir -p "$PGDATA" && chown -R postgres:postgres "$PGDATA" && chmod 700 "$PGDATA"
RUN mkdir -p /run/postgresql && chown -R postgres:postgres /run/postgresql
VOLUME /var/lib/postgresql/data

# Copy source code
COPY . /app/
WORKDIR /app

# Build the application
RUN cd /app && \
    make clean && \
    make

# Expose ports
EXPOSE 5432 9000

# Set environment variables
ENV PGHOST=localhost
ENV PGUSER=postgres
ENV PGPASSWORD=postgres
ENV PGDATABASE=postgres
ENV AWS_S3_PORT=9000

# Copy start script
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

USER postgres
# Initialize PostgreSQL database
RUN initdb -D "$PGDATA"
RUN echo "host all all 0.0.0.0/0 trust" >> "$PGDATA/pg_hba.conf" && \
    echo "listen_addresses='*'" >> "$PGDATA/postgresql.conf"

USER root
CMD ["/docker-entrypoint.sh"] 