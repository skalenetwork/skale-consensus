FROM skalenetwork/consensust_base:latest

COPY . /consensust
WORKDIR /consensust

RUN deps/build.sh

RUN cd scripts && ./build.py Debug > /tmp/log_of_build.txt   2>&1 || ( tail -1000 /tmp/log_of_build.txt && false );


#RUN cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug  -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0
#RUN bash -c "cmake --build build -- -j$(nproc)"

ENTRYPOINT ["/consensust/scripts/start.sh"]

