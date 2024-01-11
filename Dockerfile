FROM ubuntu:21.04 AS build

RUN apt-get update
RUN apt-get -y install cmake g++ git libtbb-dev

RUN mkdir /data
WORKDIR /data

ADD . /data
RUN mkdir build 
WORKDIR /data/build
RUN cmake ../ && make

# copy liblaszip.so to tmp directory
RUN ldd /data/build/PotreeConverter | grep 'liblaszip.so' | awk '{print $3}' | xargs -I '{}' cp -v '{}' /tmp/

FROM ubuntu:21.04

RUN apt-get update && apt-get -y install libtbb-dev

# copy dependencies
COPY --from=build /tmp/liblaszip.so* /usr/lib

# copy binary
COPY --from=build /data/build/PotreeConverter /data/build/PotreeConverter

# copy page template
COPY --from=build /data/build/resources /data/build/resources

ENTRYPOINT ["/data/build/PotreeConverter"]
