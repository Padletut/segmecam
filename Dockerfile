
# SegmeCam Dockerfile
# Builds and runs SegmeCam in a reproducible Linux environment

FROM nvidia/cuda:12.9.0-devel-ubuntu24.04
ENV DEBIAN_FRONTEND=noninteractive



# Install system dependencies and add-apt-repository
RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    cmake \
    git \
    python3 \
    python3-pip \
    libopencv-dev \
    libsdl2-dev \
    libgl1-mesa-dev \
    libv4l-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxi-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxfixes-dev \
    curl \
    gnupg \
    software-properties-common \
    wget \
    libnvidia-gl-575 \
    && rm -rf /var/lib/apt/lists/*

# Install Bazel (official method)
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel.gpg \
    && mv bazel.gpg /etc/apt/trusted.gpg.d/ \
    && echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" > /etc/apt/sources.list.d/bazel.list \
    && apt-get update && apt-get install -y bazel

# Optional: libv4l2loopback-dev
RUN apt-get update && apt-get install -y tzdata v4l2loopback-dkms v4l2loopback-utils && \
    ln -fs /usr/share/zoneinfo/Europe/Oslo /etc/localtime && dpkg-reconfigure -f noninteractive tzdata

# Set up workspace
WORKDIR /workspace

# Copy main workspace
COPY . /workspace

# Ensure external/ is copied (MediaPipe repo and dependencies)
COPY external/ /workspace/external/

# Copy SegmeCam source and BUILD file into MediaPipe examples directory for Bazel
RUN rm -rf /workspace/external/mediapipe/mediapipe/examples/desktop/segmecam_gui_gpu && \
    mkdir -p /workspace/external/mediapipe/mediapipe/examples/desktop/segmecam_gui_gpu && \
    cp -rf /workspace/src/segmecam_gui_gpu/* /workspace/external/mediapipe/mediapipe/examples/desktop/segmecam_gui_gpu/
#RUN ls -l /workspace/external/mediapipe/mediapipe/examples/desktop/segmecam_gui_gpu/

ENV XDG_RUNTIME_DIR=/tmp/xdg

# Set environment variables for NVIDIA OpenGL
ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu/nvidia
ENV __GLX_VENDOR_LIBRARY_NAME=nvidia

# Entrypoint: build and run using the recommended script
ENTRYPOINT ["/bin/bash", "./scripts/run_segmecam_gui_gpu.sh", "--face"]
