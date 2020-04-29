FROM skalenetwork/consensust_base:latest

COPY . /consensust
WORKDIR /consensust

ENV CC gcc-7
ENV CXX g++-7
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug

RUN cd deps; ./build.sh; cd ..
RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
RUN cmake --build build -- -j4

ENTRYPOINT ["/consensust/scripts/start.sh"]

