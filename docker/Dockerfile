# build environment
FROM ubuntu:20.04 as ubuntu

# install gcc, openSSL, git
RUN apt-get -y update
RUN apt-get -y install wget
RUN apt-get -y install git
RUN apt-get -y install build-essential
RUN apt-get -y install libssl-dev


# install cmake
RUN wget https://github.com/Kitware/CMake/releases/download/v3.19.0/cmake-3.19.0-Linux-x86_64.tar.gz
RUN tar -xzf cmake*.tar.gz 
RUN mv cmake*/ cmake/

# install tbb
RUN git clone https://github.com/wjakob/tbb.git
WORKDIR /tbb/build
RUN git checkout 141b0e310e1fb552bdca887542c9c1a8544d6503
RUN ../../cmake/bin/cmake ..
RUN make
RUN make install
WORKDIR /

# compile LAStools
RUN git clone https://github.com/LAStools/LAStools.git
WORKDIR /LAStools
RUN git checkout d8557b2d66e035e82f59dfd56abbde80e5d4dfb5
RUN make
WORKDIR /

# compile PotreeConverter
RUN git clone https://github.com/potree/PotreeConverter.git
RUN mkdir /PotreeConverter/build
WORKDIR /PotreeConverter/build
RUN git checkout da93ec26411b82ffbf799daf5ac56608ef988bf8
RUN ../../cmake/bin/cmake ..
RUN make

# copy libtbb.so in PotreeConverter build
RUN cp /tbb/build/libtbb.so /PotreeConverter/build/libtbb.so

# create link to /usr/local/bin so that callable from anywhere 
RUN ln -s /PotreeConverter/build/PotreeConverter /usr/local/bin/PotreeConverter

WORKDIR /

# command to merge all the las files together in a single potree octree:
# CMD PotreeConverter lasfiles/*.las -o lasfiles/potree/merged

# command to convert las files one by one in a separate potree octree:
CMD for f in lasfiles/*.las; do  echo "PotreeConverting $f"; PotreeConverter $f -o "lasfiles/potree/$(basename "$f" .las)"; done


# Build the docker container (may take some time):
# docker build . -t potreeconverter --network host

# Run the docker container with your las files' folder mounted inside (adapt 'path/to/lasfile/folder' to the path to your las files folder):
# docker run -it --rm -v 'path/to/lasfile/folder':/lasfiles potreeconverter 