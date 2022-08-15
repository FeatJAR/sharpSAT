FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgmp-dev
COPY . /home
WORKDIR /home
RUN cmake . -D CMAKE_BUILD_TYPE=Release
RUN make