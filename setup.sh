#!/bin/bash

NDK_PATH=/home/jason/android-ndk-r27c
PLATFORM=android-25

ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")

for ABI in "${ABIS[@]}"; do
  echo "========== Building for ABI: $ABI =========="

  BUILD_DIR=$(pwd)/build/$ABI
  LIBDATACHANNEL_PATH=$(pwd)/libdatachannel/$ABI

  cmake -B $BUILD_DIR -S . \
    -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$ABI \
    -DANDROID_PLATFORM=$PLATFORM \
    -DLIBDATACHANNEL_PATH=$LIBDATACHANNEL_PATH \
    -DLIBDATACHANNEL_INCLUDE_PATH=/home/jason/libdatachannel/include/

  cmake --build $BUILD_DIR -j$(nproc)

  echo "Done building for $ABI"
  echo
done

echo "All ABIs built successfully!"
