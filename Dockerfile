FROM ubuntu:bionic

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends g++-8 git build-essential cmake make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# gcc-8 is required for new c++17 headers used in Potree Converter
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8
RUN mkdir /data

WORKDIR /data

RUN git clone https://github.com/LAStools/LAStools.git
WORKDIR /data/LAStools/
RUN cd LASzip && cmake . -DCMAKE_BUILD_TYPE=Release \
    && make && make install && cd ../.. && rm -rf LAStools

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
