FROM docker.pkg.github.com/skalenetwork/skale-consensus/consensust_base:latest
WORKDIR /consensust




COPY . /consensust/src/
RUN cd /consensust/src; cp -rf ENGINE_VERSION *.* abstracttcpclient ./abstracttcpclient abstracttcpserver \
   blockfinalize blockproposal catchup cget chains cmake crypto datastructures db exceptions \
   headers json libjson-rpc-cpp libzmq messages monitoring network node pendingqueue pricing protocols \
   test thirdparty threads scripts utils ..
RUN rm -rf /consensust/src
ENTRYPOINT ["/consensust/scripts/start.sh"]

ENV CC gcc-7
ENV CXX g++-7
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug
RUN apt-get update; apt-get install -yq libcurl4-openssl-dev libargtable2-dev redis libhiredis-dev libmicrohttpd-dev redis-server openssl doxygen


RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
RUN cmake --build build -- -j4