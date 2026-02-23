FROM ubuntu:18.04 AS sysfilter_build
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential gdb libc6-dbg libreadline-dev \
      git openssh-client ca-certificates lsb-release patch && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keyscan -t rsa,ecdsa,ed25519 gitlab.com github.com >> /root/.ssh/known_hosts
RUN --mount=type=ssh \
    git clone --depth=1 git@gitlab.com:Egalito/sysfilter.git /workspace/sysfilter && \
    cd /workspace/sysfilter && \
    git fetch --depth=1 origin 1469319ba6ea7cab87638c1f879541e78d72d470 && \
    git checkout 1469319ba6ea7cab87638c1f879541e78d72d470 && \
    git submodule sync --recursive && \
    git submodule update --init --recursive
COPY test/src/sysfilter/egalito-patches.patch /workspace/egalito-patches.patch
COPY test/src/sysfilter/sysfilter-patches.patch /workspace/sysfilter-patches.patch
RUN cd /workspace/sysfilter/extraction/egalito && patch -p1 < /workspace/egalito-patches.patch
RUN cd /workspace/sysfilter/ && patch -p1 < /workspace/sysfilter-patches.patch
WORKDIR /workspace/sysfilter/
RUN cd extraction && make -j"$(nproc)"
# extraction/app/build_x86_64/sysfilter_extract
# libegalito.so => /workspace/sysfilter/extraction/egalito/src/libegalito.so (0x00007ff9736b2000)
# libcapstone.so.5 => /workspace/sysfilter/extraction/egalito/dep/capstone/install/lib/libcapstone.so.5 (0x00007ff9727cb000)
# libdistorm3.so => /workspace/sysfilter/extraction/egalito/dep/distorm3/make/linux/libdistorm3.so (0x00007ff9725ba000)
FROM ubuntu:18.04 AS syspart_build
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
RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keyscan -t rsa,ecdsa,ed25519 github.com >> /root/.ssh/known_hosts
RUN --mount=type=ssh \
    git clone --depth=1 git@github.com:vidyalakshmir/SysPartCode.git /workspace/SysPartCode && \
    cd /workspace/SysPartCode && \
    git fetch --depth=1 origin 6ad3ce8f01d7cab4c036235a72c8c78a7a28a0f7 && \
    git checkout 6ad3ce8f01d7cab4c036235a72c8c78a7a28a0f7 && \
    git submodule sync --recursive && \
    git submodule update --init --recursive
WORKDIR /workspace/SysPartCode
RUN ./build.sh
# analysis/app/build_x86_64/syspart
# libegalito.so => /workspace/SysPartCode/analysis/tools/egalito/src/build_x86_64/libegalito.so (0x00007fa63a2b8000)
# libcapstone.so.4 => /workspace/SysPartCode/analysis/tools/egalito/dep/capstone/install/lib/libcapstone.so.4 (0x00007fa639511000)
# libdistorm3.so => /workspace/SysPartCode/analysis/tools/egalito/dep/distorm3/make/linux/libdistorm3.so (0x00007fa639300000)
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
RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keyscan -t rsa,ecdsa,ed25519 github.com >> /root/.ssh/known_hosts
RUN --mount=type=ssh \
    git clone --depth=1 git@github.com:vidyalakshmir/SysPartCode.git /workspace/SysPartCode && \
    cd /workspace/SysPartCode && \
    git fetch --depth=1 origin 949e3339023dca94cfd2e67beefe409187e5da81 && \
    git checkout 949e3339023dca94cfd2e67beefe409187e5da81 && \
    git submodule sync --recursive && \
    git submodule update --init --recursive
COPY test/src/syspart_new/egalito-patches.patch /workspace/egalito-patches.patch
COPY test/src/syspart_new/syspart_new-patches.patch /workspace/syspart_new-patches.patch
RUN cd /workspace/SysPartCode/analysis/tools/egalito && patch -p1 < /workspace/egalito-patches.patch
RUN cd /workspace/SysPartCode && patch -p1 < /workspace/syspart_new-patches.patch
WORKDIR /workspace/SysPartCode
RUN ./build_upgraded_egalito.sh
# analysis/app/build_x86_64/syspart
# libegalito.so => /workspace/SysPartCode/analysis/tools/egalito/src/build_x86_64/libegalito.so (0x00007fa63a2b8000)
# libcapstone.so.4 => /workspace/SysPartCode/analysis/tools/egalito/dep/capstone/install/lib/libcapstone.so.4 (0x00007fa639511000)
# libdistorm3.so => /workspace/SysPartCode/analysis/tools/egalito/dep/distorm3/make/linux/libdistorm3.so (0x00007fa639300000)
FROM ubuntu:18.04 AS chestnut_clone
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git openssh-client ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keyscan -t rsa,ecdsa,ed25519 gitlab.com github.com >> /root/.ssh/known_hosts
RUN --mount=type=ssh \
    git clone --depth=1 git@github.com:isec-tugraz/Chestnut.git /workspace/chestnut && \
    cd /workspace/chestnut && \
    git fetch --depth=1 origin 839c39d4ee91c993c92fd312ca70b7d0d7bd1b83 && \
    git checkout 839c39d4ee91c993c92fd312ca70b7d0d7bd1b83
WORKDIR /workspace/chestnut/
# Binalyzer
FROM ubuntu:18.04 AS confine_clone
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git openssh-client ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keyscan -t rsa,ecdsa,ed25519 gitlab.com github.com >> /root/.ssh/known_hosts
RUN --mount=type=ssh \
    git clone --depth=1 git@github.com:shamedgh/confine.git /workspace/confine && \
    cd /workspace/confine && \
    git fetch --depth=1 origin a75dad3ad336e6258a42cd1ce4af061eea68ed76 && \
    git checkout a75dad3ad336e6258a42cd1ce4af061eea68ed76
WORKDIR /workspace/confine/
# .
FROM ubuntu:18.04 AS tests_build
SHELL ["/bin/bash", "-lc"]
RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      curl \
      git \
      openssh-client \
      make \
      cmake \
      strace \
      build-essential \
      gcc \
      g++ \
      clang \
      lld \
      llvm \
      musl-tools \
      golang-go \
      gawk; \
    rm -rf /var/lib/apt/lists/*
ENV RUST_VERSION=1.93.1
RUN set -eux; \
    curl -fsSL https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain "${RUST_VERSION}"; \
    /root/.cargo/bin/rustup toolchain install "${RUST_VERSION}"; \
    /root/.cargo/bin/rustup default "${RUST_VERSION}"; \
    /root/.cargo/bin/rustup target add x86_64-unknown-linux-musl
ENV PATH="/root/.cargo/bin:${PATH}"
WORKDIR /workspace
COPY test/build-tests.sh /workspace/test/build-tests.sh
COPY test/src/c /workspace/test/src/c
COPY test/src/cpp /workspace/test/src/cpp
COPY test/src/rust /workspace/test/src/rust
COPY test/src/go /workspace/test/src/go
COPY test/src/shared /workspace/test/src/shared
RUN set -euxo pipefail; \
    mapfile -t tests < <(find test/src/c test/src/cpp test/src/go test/src/rust -maxdepth 1 -type f \
      \( -name 't*.c' -o -name 't*.cpp' -o -name 't*.go' -o -name 't*.rs' \) \
      | sed -E 's@.*/(t[^./]+)\.[^.]+$@\1@' | sort -u); \
    for t in "${tests[@]}"; do \
      echo "=== Building test matrix for ${t} ==="; \
      bash test/build-tests.sh "$t" || { \
        echo "=== build-tests failed for ${t}; printing build logs ==="; \
        find /workspace/test/build -name '*.build.log' -maxdepth 6 -type f -print -exec tail -n +1 {} \; || true; \
        exit 1; \
      }; \
    done
# test/build/*
FROM ubuntu:18.04 AS final
SHELL ["/bin/bash", "-lc"]
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libc6-dbg \
      libstdc++6-7-dbg \
      libunwind-dev \
      python3.7 python3-pip binutils gcc libc6-dev linux-libc-dev build-essential \
      file golang-go git nginx redis-server etcd memcached apache2 && \
    rm -rf /var/lib/apt/lists/* && \
    go get -u github.com/xfernando/go2seccomp
RUN mkdir -p \
      /opt/tools/bin \
      /opt/sysfilter/lib \
      /opt/syspart/lib \
      /opt/syspart_new/lib \
      /opt/chestnut \
      /opt/confine \
      /opt/test \
      /workspace
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/app/build_x86_64/sysfilter_extract \
    /opt/tools/bin/sysfilter_extract.bin
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/src/libegalito.so \
    /opt/sysfilter/lib/
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/dep/capstone/install/lib/libcapstone.so.5 \
    /opt/sysfilter/lib/
COPY --from=sysfilter_build \
    /workspace/sysfilter/extraction/egalito/dep/distorm3/make/linux/libdistorm3.so \
    /opt/sysfilter/lib/
COPY --from=syspart_build \
    /workspace/SysPartCode/analysis/app/build_x86_64/syspart \
    /opt/tools/bin/syspart.bin
COPY --from=syspart_build \
    /workspace/SysPartCode/analysis/tools/egalito/src/build_x86_64/libegalito.so \
    /opt/syspart/lib/
COPY --from=syspart_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/capstone/install/lib/libcapstone.so.4 \
    /opt/syspart/lib/
COPY --from=syspart_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/distorm3/make/linux/libdistorm3.so \
    /opt/syspart/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/app/build_x86_64/syspart \
    /opt/tools/bin/syspart_new.bin
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/src/build_x86_64/libegalito.so \
    /opt/syspart_new/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/capstone/install/lib/libcapstone.so.4 \
    /opt/syspart_new/lib/
COPY --from=syspart_new_build \
    /workspace/SysPartCode/analysis/tools/egalito/dep/distorm3/make/linux/libdistorm3.so \
    /opt/syspart_new/lib/
COPY --from=chestnut_clone \
    /workspace/chestnut/Binalyzer \
    /opt/chestnut/Binalyzer
RUN python3 -m pip install --no-cache-dir --upgrade pip setuptools wheel
RUN python3 -m pip install --no-cache-dir -r /opt/chestnut/Binalyzer/requirements.txt
COPY --from=confine_clone \
    /workspace/confine \
    /opt/confine
COPY --from=tests_build \
    /workspace/test/build \
    /opt/test/build
RUN echo "/opt/test/build" > /etc/ld.so.conf.d/test_build.conf && ldconfig
RUN cat > /opt/tools/bin/sysfilter_extract <<'SH' && chmod +x /opt/tools/bin/sysfilter_extract
#!/bin/sh
export LD_LIBRARY_PATH=/opt/sysfilter/lib
exec /opt/tools/bin/sysfilter_extract.bin "$@"
SH
RUN cat > /opt/tools/bin/syspart <<'SH' && chmod +x /opt/tools/bin/syspart
#!/bin/sh
export LD_LIBRARY_PATH=/opt/syspart/lib
exec /opt/tools/bin/syspart.bin "$@"
SH
RUN cat > /opt/tools/bin/syspart_new <<'SH' && chmod +x /opt/tools/bin/syspart_new
#!/bin/sh
export LD_LIBRARY_PATH=/opt/syspart_new/lib
exec /opt/tools/bin/syspart_new.bin "$@"
SH
COPY test/src/chestnut/main.py /opt/chestnut/Binalyzer/main.py
COPY test/src/confine/main.py /opt/confine/main.py
COPY test/run-test.sh /workspace/run-test.sh
RUN chmod 0700 /workspace/run-test.sh
ENV PATH="/opt/tools/bin:${PATH}"
WORKDIR /workspace
CMD ["/bin/bash"]
