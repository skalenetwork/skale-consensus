FROM docker.pkg.github.com/skalenetwork/skale-consensus/consensust_base:latest
WORKDIR /consensust
COPY ENGINE_VERSION ./
COPY .clang-format ./
COPY *.yml ./
COPY *.cmake ./
COPY *.cpp ./
COPY *.h ./
COPY *.txt ./
COPY *.c ./
COPY *.sh ./
COPY *.m4 ./
COPY *.gmp ./
COPY *.ac ./
COPY *.json ./
COPY abstracttcpclient ./abstracttcpclient
COPY abstracttcpserver ./abstracttcpserver
COPY  blockfinalize ./blockfinalize
COPY blockproposal ./blockproposal
COPY catchup ./catchup
COPY cget ./cget
COPY chains ./chains
COPY cmake ./cmake
COPY crypto ./crypto
COPY datastructures ./datastructures
COPY db ./db
COPY exceptions ./exceptions
COPY headers ./headers
COPY json ./json
COPY messages ./messages
COPY monitoring ./monitoring
COPY network ./network
COPY node ./node
COPY pendingqueue ./pendingqueue
COPY pricing ./pricing
COPY protocols ./protocols
COPY test ./test
COPY thirdparty ./thirdparty
COPY threads ./threads
COPY utils ./utils

ENV CC gcc-7
ENV CXX g++-7
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug

RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
RUN cmake --build build -- -j4