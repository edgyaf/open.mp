FROM ubuntu:22.04
RUN \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y \
        cmake \
        ninja-build \
        clang-11 \
        python3-pip \
        gcc-9-multilib \
        g++-9-multilib \
        libstdc++-11-dev:i386 \
    && \
    useradd -m user && \
    su user -c 'pip3 install --user conan'

USER user

# Rust, for components ported from C++. Keep in sync with rust-toolchain.toml.
RUN wget -qO- https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain 1.99.0         --target i686-unknown-linux-gnu --target x86_64-unknown-linux-gnu

ENV CC=/usr/bin/clang-11 \
    CXX=/usr/bin/clang++-11 \
    PATH=/home/user/.cargo/bin:~/.local/bin:${PATH}

COPY docker-entrypoint.sh /
CMD /docker-entrypoint.sh
