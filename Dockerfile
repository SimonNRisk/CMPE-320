FROM gcc:latest

WORKDIR /app

COPY . .

RUN apt-get update && apt-get install -y \
    cmake \
    git \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

RUN cmake -S . -B build
RUN cmake --build build

CMD ["./build/server"]