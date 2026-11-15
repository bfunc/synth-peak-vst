# Linux build environment for compile checks and the panel snapshot tool (the release builds come from Windows CI).
FROM gcc:14
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build pkg-config git \
    libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxext-dev libxcomposite-dev libgl1-mesa-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*
