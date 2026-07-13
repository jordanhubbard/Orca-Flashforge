set(DEP_CMAKE_OPTS "-DCMAKE_POSITION_INDEPENDENT_CODE=ON" "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
set(DEP_BOOST_TOOLSET "gcc")
set(DEP_BITS 64)

find_package(Git REQUIRED)

# TODO make sure to build tbb with -flifetime-dse=1
include("deps-unix-common.cmake")
