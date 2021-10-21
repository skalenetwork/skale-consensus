FROM skalenetwork/consensust_base:latest

COPY . /consensust
WORKDIR /consensust

RUN deps/clean.sh
RUN deps/build.sh




RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
RUN bash -c "cmake --build build -- -j$(nproc)"

ENTRYPOINT ["/consensust/scripts/start.sh"]

