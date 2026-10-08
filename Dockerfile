FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config python3 \
    libopencv-dev libtesseract-dev libleptonica-dev tesseract-ocr-eng \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY CMakeLists.txt ./
COPY src/ src/
COPY tests/ tests/
COPY data/ data/
COPY test_images/ test_images/
COPY ground_truth.csv ./
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    && cmake --build build --parallel 2 \
    && ctest --test-dir build --output-on-failure
CMD ["sh", "-c", "./build/plate_recognizer test_images - output/results.csv && ./build/evaluate output/results.csv ground_truth.csv output/errors.csv"]
