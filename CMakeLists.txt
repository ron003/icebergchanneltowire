

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

daq_setup_environment()


find_package(Boost COMPONENTS unit_test_framework REQUIRED)

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_application

daq_add_application(channel-to-wire channel-to-wire.cxx LINK_LIBRARIES TRACE::TRACE) # Any libraries to link in not yet determined

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps"
#    "${CMAKE_CURRENT_SOURCE_DIR}/apps/larcoreobj"
)

##############################################################################


# See https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-cmake/#daq_add_unit_test

daq_add_unit_test(Placeholder_test LINK_LIBRARIES)  # Placeholder_test should be replaced with real unit tests

##############################################################################

daq_install()

