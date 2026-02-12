#!/bin/bash
set -e
TRACEE_CONTAINER_ID=$1
TRACEE_LOG_PATH=/var/log/tracee/tracee.json
MAX_MEMORY_MB=28000
RESULT_PROFILES_DIR=/srv/programms/constantin/.results
STATIC_TOOLS=confine,sysfilter,syspart,go2seccomp

COLOR_RESET='\033[0m'
COLOR_BLUE='\033[0;34m'
COLOR_YELLOW='\033[1;33m'
COLOR_RED='\033[0;31m'

exec 3>&2
exec 2> >(while IFS= read -r line; do [[ -n "$line" ]] && printf "${COLOR_RED}[ERROR] ${COLOR_RESET}%s\n"  "$line" >&3; done)
exec 1> >(while IFS= read -r line; do [[ -n "$line" ]] && printf "${COLOR_BLUE}[INFO] ${COLOR_RESET}%s\n" "$line" >&3; done)

log_info()  { printf "${COLOR_BLUE}[INFO] ${COLOR_RESET}%s\n"  "$*" >&3; }
log_warn()  { printf "${COLOR_YELLOW}[WARN] ${COLOR_RESET}%s\n"  "$*" >&3; }
log_error() { printf "${COLOR_RED}[ERROR] ${COLOR_RESET}%s\n" "$*" >&3; }

container_alive() {
    local status
    status=$(docker inspect --format='{{.State.Status}}' "$1" 2>/dev/null) || return 1
    [[ "$status" == "running" ]]
}

remove_container() {
    local id=$1
    if container_alive "$id"; then
        docker rm -f "$id" >/dev/null
    else
        log_warn "container ${id:0:12} is not running, skipping removal"
    fi
}

RUNNING_CONTAINERS=()

cleanup() {
    trap - EXIT INT TERM
    log_warn 'cleaning up containers...'
    for id in "${RUNNING_CONTAINERS[@]}"; do
        remove_container "$id" || true
    done
}
trap cleanup EXIT INT TERM

run_constantin() {
    local container_id=$1
    local profile_name=$2
    docker run --rm -it \
        -v /var/run/docker.sock:/var/run/docker.sock \
        -v ${TRACEE_LOG_PATH}:${TRACEE_LOG_PATH} \
        -v ${RESULT_PROFILES_DIR}:/workspace/results \
        -m ${MAX_MEMORY_MB}m \
        tools:latest constant-in \
        --mode=both \
        --container=${container_id} \
        --static-tools=${STATIC_TOOLS} \
        --syspart-icanalysis \
        --dynamic-tools=tracee \
        --tracee-log=${TRACEE_LOG_PATH} \
        --auto-choice \
        --log-level=info \
        --seccomp-output=/workspace/results/seccomp_${profile_name}.json \
        --seccomp-per-instrument=true \
        --output=/workspace/results/result_${profile_name}.json \
        >&3 2>&3
}

REDIS_IMAGE=redis:latest
log_info 'starting redis (unconfined)'
REDIS_ID=$(docker run --rm -d --name bench_redis --security-opt seccomp=unconfined --network host $REDIS_IMAGE redis-server --port 6399 --loglevel warning)
log_info 'waiting for startup...'
sleep 10
RUNNING_CONTAINERS+=("${REDIS_ID}")
log_info 'running constantin (idle)'
run_constantin ${REDIS_ID} redis_idle
log_info 'running redis benchmark'
docker run --rm --network host ${REDIS_IMAGE} redis-benchmark -q -h 127.0.0.1 -p 6399 -c 50 -n 50000 -d 64
log_info 'running constantin (bench)'
run_constantin ${REDIS_ID} redis_bench
log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null
remove_container ${REDIS_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$REDIS_ID}")
log_info 'starting redis with idle seccomp profile'
REDIS_ID=$(docker run --rm -d --name bench_redis --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_redis_idle.json --network host ${REDIS_IMAGE} redis-server --port 6399 --loglevel warning)
RUNNING_CONTAINERS+=("${REDIS_ID}")
log_info 'waiting for startup...'
sleep 10
log_info 'running redis benchmark (with idle seccomp profile)'
docker run --rm --network host ${REDIS_IMAGE} redis-benchmark -q -h 127.0.0.1 -p 6399 -c 50 -n 50000 -d 64
remove_container ${REDIS_ID}
log_info 'done'

POSTGRES_IMAGE=postgres:latest
log_info 'starting postgres (unconfined)'
PG_ID=$(docker run --rm -d --name bench_postgresql --security-opt seccomp=unconfined --network host -e POSTGRES_PASSWORD=bench -e POSTGRES_DB=benchdb ${POSTGRES_IMAGE})
log_info 'waiting for startup...'
sleep 10
RUNNING_CONTAINERS+=("${PG_ID}")
log_info 'running constantin (idle)'
run_constantin ${PG_ID} postgres_idle
log_info 'running postgres benchmark'
docker run --rm --network host -e PGPASSWORD=bench ${POSTGRES_IMAGE} pgbench -h 127.0.0.1 -p 5432 -U postgres -d benchdb -i
docker run --rm --network host -e PGPASSWORD=bench ${POSTGRES_IMAGE} pgbench -h 127.0.0.1 -p 5432 -U postgres -d benchdb -c 10 -j 2 -T 30
log_info 'running constantin (bench)'
run_constantin ${PG_ID} postgres_bench
log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null
remove_container ${PG_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$PG_ID}")
log_info 'starting postgres with idle seccomp profile'
PG_ID=$(docker run --rm -d --name bench_postgresql --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_postgres_idle.json --network host -e POSTGRES_PASSWORD=bench -e POSTGRES_DB=benchdb ${POSTGRES_IMAGE})
RUNNING_CONTAINERS+=("${PG_ID}")
log_info 'waiting for startup...'
sleep 10
log_info 'running postgres benchmark (with idle seccomp profile)'
docker run --rm --network host -e PGPASSWORD=bench ${POSTGRES_IMAGE} pgbench -h 127.0.0.1 -p 5432 -U postgres -d benchdb -i
docker run --rm --network host -e PGPASSWORD=bench ${POSTGRES_IMAGE} pgbench -h 127.0.0.1 -p 5432 -U postgres -d benchdb -c 10 -j 2 -T 30
remove_container ${PG_ID}
log_info 'done'

MONGODB_IMAGE=mongo:4.4.6
log_info 'starting mongodb (unconfined)'
MONGODB_ID=$(docker run --rm -d --name bench_mongodb --security-opt seccomp=unconfined --network host ${MONGODB_IMAGE})
log_info 'waiting for startup...'
sleep 10
RUNNING_CONTAINERS+=("${MONGODB_ID}")
log_info 'running constantin (idle)'
run_constantin ${MONGODB_ID} mongodb_idle
log_info 'running mongodb benchmark'
docker run --rm --network host mongo-perf -f testcases/simple_insert.js -t 1 --trialTime 10
log_info 'running constantin (bench)'
run_constantin ${MONGODB_ID} mongodb_bench
log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null
remove_container ${MONGODB_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$MONGODB_ID}")
log_info 'starting mongodb with idle seccomp profile'
MONGODB_ID=$(docker run --rm -d --name bench_mongodb --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_mongodb_idle.json --network host ${MONGODB_IMAGE})
RUNNING_CONTAINERS+=("${MONGODB_ID}")
log_info 'waiting for startup...'
sleep 10
log_info 'running mongodb benchmark (with idle seccomp profile)'
docker run --rm --network host mongo-perf -f testcases/simple_insert.js -t 1 --trialTime 10
remove_container ${MONGODB_ID}
log_info 'done'

MYSQL_IMAGE=mysql:8.0
log_info 'starting mysql (unconfined)'
MYSQL_ID=$(docker run -d --rm --network host --security-opt seccomp=unconfined --name mysql-test -e MYSQL_ROOT_PASSWORD=testpass -e MYSQL_DATABASE=sbtest ${MYSQL_IMAGE} --default-authentication-plugin=mysql_native_password)
log_info 'waiting for startup...'
sleep 10
RUNNING_CONTAINERS+=("${MYSQL_ID}")
log_info 'running constantin (idle)'
run_constantin ${MYSQL_ID} mysql_idle
log_info 'running mysql benchmark'
docker run -it --rm --network host severalnines/sysbench sysbench oltp_read_write --mysql-host=127.0.0.1 --mysql-user=root --mysql-password=testpass --mysql-db=sbtest --tables=10 --table-size=10000 prepare
docker run -it --rm --network host severalnines/sysbench sysbench oltp_read_write --mysql-host=127.0.0.1 --mysql-user=root --mysql-password=testpass --mysql-db=sbtest --tables=10 --table-size=10000 --threads=16 --time=60 run
log_info 'running constantin (bench)'
run_constantin ${MYSQL_ID} mysql_bench
log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null
remove_container ${MYSQL_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$MYSQL_ID}")
log_info 'starting mysql with idle seccomp profile'
MYSQL_ID=$(docker run -d --rm --network host --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_mysql_idle.json --name mysql-test -e MYSQL_ROOT_PASSWORD=testpass -e MYSQL_DATABASE=sbtest ${MYSQL_IMAGE} --default-authentication-plugin=mysql_native_password)
RUNNING_CONTAINERS+=("${MYSQL_ID}")
log_info 'waiting for startup...'
sleep 40
log_info 'running mysql benchmark (with idle seccomp profile)'
docker run -it --rm --network host severalnines/sysbench sysbench oltp_read_write --mysql-host=127.0.0.1 --mysql-user=root --mysql-password=testpass --mysql-db=sbtest --tables=10 --table-size=10000 prepare
docker run -it --rm --network host severalnines/sysbench sysbench oltp_read_write --mysql-host=127.0.0.1 --mysql-user=root --mysql-password=testpass --mysql-db=sbtest --tables=10 --table-size=10000 --threads=16 --time=60 run
remove_container ${MYSQL_ID}
log_info 'done'

WRK_IMAGE=williamyeh/wrk
WHOAMI_IMAGE=traefik/whoami
BACKEND_PORT=18080
WRK_THREADS=5
WRK_CONNECTIONS=500
WRK_DURATION=30s

log_info 'starting whoami backend for proxy benchmarks'
WHOAMI_ID=$(docker run --rm -d \
    --name bench_whoami \
    --network host \
    --security-opt seccomp=unconfined \
    -e WHOAMI_PORT_NUMBER=${BACKEND_PORT} \
    ${WHOAMI_IMAGE} \
    --port ${BACKEND_PORT})
RUNNING_CONTAINERS+=("${WHOAMI_ID}")
sleep 2

NGINX_IMAGE=nginx:latest
NGINX_PROXY_PORT=18081

log_info 'writing nginx proxy config'
cat > /tmp/bench_nginx.conf << EOF
worker_processes auto;
events { worker_connections 10000; }
http {
    upstream backend {
        server 127.0.0.1:${BACKEND_PORT};
        keepalive 300;
    }
    server {
        listen ${NGINX_PROXY_PORT};
        access_log off;
        location / {
            proxy_pass http://backend;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
        }
    }
}
EOF

log_info 'starting nginx proxy (unconfined)'
NGINX_ID=$(docker run --rm -d \
    --name bench_nginx \
    --network host \
    --security-opt seccomp=unconfined \
    -v /tmp/bench_nginx.conf:/etc/nginx/nginx.conf:ro \
    ${NGINX_IMAGE})
RUNNING_CONTAINERS+=("${NGINX_ID}")
sleep 3

log_info 'running constantin (nginx idle)'
run_constantin ${NGINX_ID} nginx_idle

log_info 'running nginx benchmark'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${NGINX_PROXY_PORT}/

log_info 'running constantin (nginx bench)'
run_constantin ${NGINX_ID} nginx_bench

log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null

remove_container ${NGINX_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$NGINX_ID}")

log_info 'starting nginx with idle seccomp profile'
NGINX_ID=$(docker run --rm -d \
    --name bench_nginx \
    --network host \
    --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_nginx_idle.json \
    -v /tmp/bench_nginx.conf:/etc/nginx/nginx.conf:ro \
    ${NGINX_IMAGE})
RUNNING_CONTAINERS+=("${NGINX_ID}")
sleep 3

log_info 'running nginx benchmark (with idle seccomp profile)'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${NGINX_PROXY_PORT}/

remove_container ${NGINX_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$NGINX_ID}")
log_info 'nginx done'

HAPROXY_IMAGE=haproxy:latest
HAPROXY_PORT=18082

log_info 'writing haproxy config'
cat > /tmp/bench_haproxy.cfg << EOF
global
    maxconn 50000

defaults
    mode http
    timeout connect 5s
    timeout client  30s
    timeout server  30s
    option http-server-close
    option forwardfor

frontend http_front
    bind *:${HAPROXY_PORT}
    default_backend servers

backend servers
    balance roundrobin
    option http-keep-alive
    server backend1 127.0.0.1:${BACKEND_PORT} check
EOF

log_info 'starting haproxy (unconfined)'
HAPROXY_ID=$(docker run --rm -d \
    --name bench_haproxy \
    --network host \
    --security-opt seccomp=unconfined \
    -v /tmp/bench_haproxy.cfg:/usr/local/etc/haproxy/haproxy.cfg:ro \
    ${HAPROXY_IMAGE})
RUNNING_CONTAINERS+=("${HAPROXY_ID}")
sleep 3

log_info 'running constantin (haproxy idle)'
run_constantin ${HAPROXY_ID} haproxy_idle

log_info 'running haproxy benchmark'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${HAPROXY_PORT}/

log_info 'running constantin (haproxy bench)'
run_constantin ${HAPROXY_ID} haproxy_bench

log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null

remove_container ${HAPROXY_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$HAPROXY_ID}")

log_info 'starting haproxy with idle seccomp profile'
HAPROXY_ID=$(docker run --rm -d \
    --name bench_haproxy \
    --network host \
    --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_haproxy_idle.json \
    -v /tmp/bench_haproxy.cfg:/usr/local/etc/haproxy/haproxy.cfg:ro \
    ${HAPROXY_IMAGE})
RUNNING_CONTAINERS+=("${HAPROXY_ID}")
sleep 3

log_info 'running haproxy benchmark (with idle seccomp profile)'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${HAPROXY_PORT}/

remove_container ${HAPROXY_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$HAPROXY_ID}")
log_info 'haproxy done'

TRAEFIK_IMAGE=traefik:latest
TRAEFIK_PORT=18084

log_info 'writing traefik static config'
cat > /tmp/bench_traefik.yml << EOF
entryPoints:
  web:
    address: ":${TRAEFIK_PORT}"

providers:
  file:
    filename: /etc/traefik/dynamic.yml

api:
  insecure: false
EOF

log_info 'writing traefik dynamic config'
cat > /tmp/bench_traefik_dynamic.yml << EOF
http:
  routers:
    bench:
      rule: "PathPrefix(\`/\`)"
      entryPoints:
        - web
      service: whoami

  services:
    whoami:
      loadBalancer:
        servers:
          - url: "http://127.0.0.1:${BACKEND_PORT}"
EOF

log_info 'starting traefik (unconfined)'
TRAEFIK_ID=$(docker run --rm -d \
    --name bench_traefik \
    --network host \
    --security-opt seccomp=unconfined \
    -v /tmp/bench_traefik.yml:/etc/traefik/traefik.yml:ro \
    -v /tmp/bench_traefik_dynamic.yml:/etc/traefik/dynamic.yml:ro \
    ${TRAEFIK_IMAGE})
RUNNING_CONTAINERS+=("${TRAEFIK_ID}")
sleep 5

log_info 'running constantin (traefik idle)'
run_constantin ${TRAEFIK_ID} traefik_idle

log_info 'running traefik benchmark'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${TRAEFIK_PORT}/

log_info 'running constantin (traefik bench)'
run_constantin ${TRAEFIK_ID} traefik_bench

log_info 'clearing tracee logs'
echo '' | sudo tee ${TRACEE_LOG_PATH} >/dev/null
log_info 'restarting tracee'
docker restart ${TRACEE_CONTAINER_ID} >/dev/null

remove_container ${TRAEFIK_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$TRAEFIK_ID}")

log_info 'starting traefik with idle seccomp profile'
TRAEFIK_ID=$(docker run --rm -d \
    --name bench_traefik \
    --network host \
    --security-opt seccomp=${RESULT_PROFILES_DIR}/seccomp_traefik_idle.json \
    -v /tmp/bench_traefik.yml:/etc/traefik/traefik.yml:ro \
    -v /tmp/bench_traefik_dynamic.yml:/etc/traefik/dynamic.yml:ro \
    ${TRAEFIK_IMAGE})
RUNNING_CONTAINERS+=("${TRAEFIK_ID}")
sleep 5

log_info 'running traefik benchmark (with idle seccomp profile)'
docker run --rm --network host ${WRK_IMAGE} \
    -t${WRK_THREADS} -c${WRK_CONNECTIONS} -d${WRK_DURATION} --latency \
    http://127.0.0.1:${TRAEFIK_PORT}/

remove_container ${TRAEFIK_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$TRAEFIK_ID}")
log_info 'traefik done'

remove_container ${WHOAMI_ID}
RUNNING_CONTAINERS=("${RUNNING_CONTAINERS[@]/$WHOAMI_ID}")
log_info 'all proxy benchmarks done'