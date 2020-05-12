FROM ubuntu:bionic

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y g++-8 git cmake libboost-all-dev
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8
RUN mkdir /data

WORKDIR /data

RUN git clone https://github.com/LAStools/LAStools.git
WORKDIR /data/LAStools/
RUN make && cd LASzip && cmake . -DCMAKE_BUILD_TYPE=Release \
    && make && make install

RUN mkdir ./PotreeConverter
WORKDIR /data/PotreeConverter
ADD . /data/PotreeConverter
RUN mkdir build && cd build && cmake \
            -DCMAKE_CXX_STANDARD=17 \
            -DCMAKE_CXX_STANDARD_REQUIRED=ON \
            -DCMAKE_BUILD_TYPE=Release \
            -DLASZIP_INCLUDE_DIRS=/usr/local/include/laszip/ \
            -DLASZIP_LIBRARY=/usr/local/lib/liblaszip.so .. \  
    && make
