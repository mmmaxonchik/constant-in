FROM ubuntu:18.04 AS sysfilter_build
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential gdb libc6-dbg libreadline-dev \
      git openssh-client ca-certificates lsb-release patch && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY tools/sysfilter sysfilter
WORKDIR /workspace/sysfilter/
RUN cd extraction && make -j"$(nproc)"
FROM ubuntu:18.04 AS syspart_new_build
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      make g++ build-essential \
      libreadline-dev gdb lsb-release unzip \
      libc6-dbg libstdc++6-7-dbg \
      libunwind-dev python3 \
      git openssh-client ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY tools/SysPartCode SysPartCode
WORKDIR /workspace/SysPartCode
RUN ./build_upgraded_egalito.sh
FROM ubuntu:18.04 AS go_build
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      wget ca-certificates build-essential libreadline-dev && \
    rm -rf /var/lib/apt/lists/*
RUN wget -q https://go.dev/dl/go1.22.5.linux-amd64.tar.gz -O /tmp/go.tar.gz && \
    tar -C /usr/local -xzf /tmp/go.tar.gz && \
    rm /tmp/go.tar.gz
ENV PATH="/usr/local/go/bin:${PATH}"
RUN mkdir -p /opt/headers
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/app/src/sysfilter_lib.h \
    /opt/headers/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/app/src/syspart_lib.h \
    /opt/headers/
RUN mkdir -p /opt/sysfilter/lib /opt/syspart_new/lib
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/app/build_x86_64/libsysfilter_analysis.so \
    /opt/sysfilter/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/app/build_x86_64/libsyspart_analysis.so \
    /opt/syspart_new/lib/
RUN echo "/opt/sysfilter/lib"  > /etc/ld.so.conf.d/sysfilter.conf && \
    echo "/opt/syspart_new/lib" > /etc/ld.so.conf.d/syspart.conf && \
    ldconfig
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY cmd/ ./cmd/
COPY src/ ./src/
RUN CGO_ENABLED=1 \
    CGO_CFLAGS="-I/opt/headers" \
    CGO_LDFLAGS="-L/opt/sysfilter/lib -lsysfilter_analysis" \
    go build -o /usr/local/bin/constant-in-sysfilter-helper ./cmd/constant-in-sysfilter-helper/
RUN CGO_ENABLED=1 \
    CGO_CFLAGS="-I/opt/headers" \
    CGO_LDFLAGS="-L/opt/syspart_new/lib -lsyspart_analysis" \
    go build -o /usr/local/bin/constant-in-syspart-helper ./cmd/constant-in-syspart-helper/
RUN CGO_ENABLED=0 \
    go build -o /usr/local/bin/constant-in ./cmd/constant-in/
FROM ubuntu:18.04 AS final
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libc6-dbg libstdc++6-7-dbg libunwind-dev \
      binutils gcc libc6-dev linux-libc-dev build-essential file gdb \
      libreadline7 python3 ca-certificates \
      libjemalloc1 && \
    rm -rf /var/lib/apt/lists/*
RUN mkdir -p \
      /opt/tools/bin \
      /opt/sysfilter/lib \
      /opt/syspart_new/lib \
      /workspace
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/app/build_x86_64/sysfilter_extract \
    /opt/tools/bin/sysfilter_extract.bin
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/app/build_x86_64/libsysfilter_analysis.so \
    /opt/sysfilter/lib/
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/src/build_x86_64/libegalito.so \
    /opt/sysfilter/lib/
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/dep/capstone/install/lib/libcapstone.so.5 \
    /opt/sysfilter/lib/
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/dep/distorm3/make/linux/libdistorm3.so \
    /opt/sysfilter/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/app/build_x86_64/syspart \
    /opt/tools/bin/syspart_new.bin
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/app/build_x86_64/libsyspart_analysis.so \
    /opt/syspart_new/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/src/build_x86_64/libegalito.so \
    /opt/syspart_new/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/capstone/install/lib/libcapstone.so.4 \
    /opt/syspart_new/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/distorm3/make/linux/libdistorm3.so \
    /opt/syspart_new/lib/
COPY --from=go_build /usr/local/bin/constant-in /opt/tools/bin/constant-in.bin
COPY --from=go_build /usr/local/bin/constant-in-sysfilter-helper /opt/tools/bin/constant-in-sysfilter-helper
COPY --from=go_build /usr/local/bin/constant-in-syspart-helper /opt/tools/bin/constant-in-syspart-helper
RUN echo "/opt/sysfilter/lib"   > /etc/ld.so.conf.d/sysfilter.conf && \
    echo "/opt/syspart_new/lib" > /etc/ld.so.conf.d/syspart.conf && \
    ldconfig
RUN cat > /opt/tools/bin/sysfilter_extract <<'SH' && chmod +x /opt/tools/bin/sysfilter_extract
#!/bin/sh
export LD_LIBRARY_PATH=/opt/sysfilter/lib
exec /opt/tools/bin/sysfilter_extract.bin "$@"
SH
RUN cat > /opt/tools/bin/syspart_new <<'SH' && chmod +x /opt/tools/bin/syspart_new
#!/bin/sh
export LD_LIBRARY_PATH=/opt/syspart_new/lib
exec /opt/tools/bin/syspart_new.bin "$@"
SH
RUN cat > /opt/tools/bin/constant-in <<'SH' && chmod +x /opt/tools/bin/constant-in
#!/bin/sh
export LD_LIBRARY_PATH=/opt/sysfilter/lib:/opt/syspart_new/lib
export MALLOC_CONF="lg_dirty_mult:0"
exec /opt/tools/bin/constant-in.bin "$@"
SH
COPY tools/confine /opt/confine
ENV PATH="/opt/tools/bin:${PATH}"
WORKDIR /workspace
CMD ["/bin/bash"]
