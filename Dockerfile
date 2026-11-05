FROM gcc:latest

WORKDIR /app

COPY backend/ ./backend/

RUN apt-get update && apt-get install -y \
    cmake \
    git \
    libasio-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN cmake -S backend -B build
RUN cmake --build build

CMD ["./build/server"]