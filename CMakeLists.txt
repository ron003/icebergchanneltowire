

# This is a skeleton CMakeLists.txt file, auto-generated on
# Mon Mar 2 13:43:16 CST 2026.  The developer(s) of this package should delete
# this comment as well as adding dependent targets, packages,
# etc. specific to the package. For details on how to write a package,
# please see
# https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/

cmake_minimum_required(VERSION 3.12)
project(icebergchanneltowire VERSION 0.0.0)

find_package(daq-cmake REQUIRED)
find_package(TRACE REQUIRED)
find_package(PNG REQUIRED)
find_package(fddetdataformats REQUIRED)
find_package(detchannelmaps REQUIRED)

daq_setup_environment()


find_package(Boost COMPONENTS unit_test_framework REQUIRED)

# GPU support: requires ICEBERG_GPU env var AND CUDAToolkit installed
set(ICEBERG_GPU_ENABLED FALSE)
if(DEFINED ENV{ICEBERG_GPU})
  find_package(CUDAToolkit QUIET)
  if(CUDAToolkit_FOUND)
    set(ICEBERG_GPU_ENABLED TRUE)
    enable_language(CUDA)
    message(STATUS "ICEBERG_GPU set and CUDAToolkit found — GPU targets enabled")
  else()
    message(FATAL_ERROR "ICEBERG_GPU is set but CUDAToolkit was not found")
  endif()
endif()

##############################################################################

add_library(icebergwirechannelmap STATIC apps/IcebergWireChannelMap.cpp)
target_include_directories(icebergwirechannelmap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/apps")
target_link_libraries(icebergwirechannelmap PUBLIC TRACE::TRACE)

# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_application

daq_add_application(channel-to-wire channel-to-wire.cxx LINK_LIBRARIES TRACE::TRACE icebergwirechannelmap)

daq_add_application(images-to-pcap 
    images-to-pcap.cxx 
    PngImageLoader.cpp 
    PcapWriter.cpp 
    LINK_LIBRARIES TRACE::TRACE PNG::PNG detchannelmaps::detchannelmaps fddetdataformats::fddetdataformats icebergwirechannelmap)

daq_add_application(pcap-to-images 
    pcap-to-images.cxx 
    PngImageLoader.cpp 
    PcapReader.cpp 
    LINK_LIBRARIES TRACE::TRACE PNG::PNG detchannelmaps::detchannelmaps fddetdataformats::fddetdataformats icebergwirechannelmap)

if(ICEBERG_GPU_ENABLED)
  daq_add_application(pcap-to-images2
      pcap-to-images2.cxx
      pcap-to-images2-kernel.cu
      PngImageLoader.cpp
      PcapReader.cpp
      LINK_LIBRARIES TRACE::TRACE PNG::PNG detchannelmaps::detchannelmaps fddetdataformats::fddetdataformats icebergwirechannelmap CUDA::cudart)
  target_compile_definitions(pcap-to-images2 PRIVATE HAVE_ICEBERG_GPU)
  # Suppress "style of line directive is a GCC extension" from nvcc's preprocessor output.
  # nvcc forwards -pedantic to the host compiler via -forward-unknown-to-host-compiler;
  # we must cancel it through the same path (not -Xcompiler) so it applies to all phases.
  target_compile_options(pcap-to-images2 PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Wno-pedantic>)
else()
  daq_add_application(pcap-to-images2
      pcap-to-images2.cxx
      PngImageLoader.cpp
      PcapReader.cpp
      LINK_LIBRARIES TRACE::TRACE PNG::PNG detchannelmaps::detchannelmaps fddetdataformats::fddetdataformats icebergwirechannelmap)
endif()

daq_add_application(dune-pcap
    dune-pcap.cxx 
    LINK_LIBRARIES TRACE::TRACE fddetdataformats::fddetdataformats icebergwirechannelmap)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps"
    "${CMAKE_CURRENT_SOURCE_DIR}/../sourcecode/fddetdataformats/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../sourcecode/detdataformats/include"
#    "${CMAKE_CURRENT_SOURCE_DIR}/apps/larcoreobj"
)

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_unit_test

daq_add_unit_test(Placeholder_test LINK_LIBRARIES)  # Placeholder_test should be replaced with real unit tests

##############################################################################

daq_install()

