# Use Ubuntu base
FROM ubuntu:24.04

# Set environment variables to avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    g++-14 \
    libboost-all-dev \
	libtbb-dev \
    python3 \
    python3-venv \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Set up virtual environment
ENV VENV_PATH=/opt/venv
RUN python3 -m venv $VENV_PATH
ENV PATH="$VENV_PATH/bin:$PATH"

# Install Python packages into the venv
RUN pip install --upgrade pip && \
    pip install numpy laspy scipy

COPY . /app

# Create a working directory
WORKDIR /app

# Compile PotreeConverter
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_CXX_COMPILER=g++-14 .. && \
    make -j$(nproc)

# Add binary to PATH
ENV PATH="/app/build:${PATH}"