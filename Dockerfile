FROM skalenetwork/consensust_base:latest

RUN apt-get install -yq software-properties-common
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update
RUN apt-get install -y software-properties-common; sudo apt-add-repository universe; apt-get update; \
    apt-get install -yq  libprocps-dev gcc-9 g++-9 valgrind gawk sed libffi-dev ccache libgoogle-perftools-dev \
    flex bison yasm texinfo autotools-dev automake \
    python3 python3-pip \
    cmake libtool build-essential pkg-config autoconf wget git  libargtable2-dev \
    libmicrohttpd-dev libhiredis-dev redis-server openssl libssl-dev doxygen idn2 \
    libgcrypt20-dev rustc
    # python python-pip

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 9
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 9
RUN update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-9 9
RUN update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-9 9
RUN update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-9 9

COPY . /consensust
WORKDIR /consensust

ENV CC gcc-9
ENV CXX g++-9
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug

RUN deps/clean.sh
RUN deps/build.sh DEBUG=1 PARALLEL_COUNT=$(nproc)

RUN rm -rf ./build/*
RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
RUN bash -c "cmake --build build -- -j$(nproc)"

ENTRYPOINT ["/consensust/scripts/start.sh"]
