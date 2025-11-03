FROM ubuntu:24.04 AS base
RUN apt-get update && \
	apt-get install -y cmake g++ libtbb-dev && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY ./Converter /app/Converter
COPY ./resources /app/resources
COPY ./CMakeLists.txt /app/
COPY ./LICENSE /app/
COPY ./README.md /app/
RUN cmake -S . -B build && \
	cmake --build build --config Release
WORKDIR /app/build
RUN make
FROM ubuntu:24.04 AS runtime
WORKDIR /app
RUN apt-get update && \
	apt-get install -y libtbb-dev && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/*
COPY --from=base /app/build /app/build
ENTRYPOINT ["/app/build/PotreeConverter"]