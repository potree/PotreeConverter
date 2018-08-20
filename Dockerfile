FROM ubuntu:18.04

RUN apt-get update && apt-get install -y g++ git cmake libboost-all-dev gosu
RUN mkdir -p /data

WORKDIR /data

RUN git clone https://github.com/m-schuetz/LAStools.git
WORKDIR /data/LAStools/LASzip
RUN mkdir -p build
RUN cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cd build && make

RUN mkdir -p ./PotreeConverter
WORKDIR /data/PotreeConverter
ADD . /data/PotreeConverter
RUN mkdir -p build
RUN cd build && cmake -DCMAKE_BUILD_TYPE=Release -DLASZIP_INCLUDE_DIRS=/data/LAStools/LASzip/dll -DLASZIP_LIBRARY=/data/LAStools/LASzip/build/src/liblaszip.so .. 
RUN cd build && make
RUN cp -R /data/PotreeConverter/PotreeConverter/resources/ /data
RUN mkdir -p /input && mkdir -p /output

ENTRYPOINT ["/data/PotreeConverter/entrypoint.sh"]
