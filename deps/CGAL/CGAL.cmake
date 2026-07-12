orcaslicer_add_cmake_project(
    CGAL
    # GIT_REPOSITORY https://github.com/CGAL/cgal.git
    # GIT_TAG        bec70a6d52d8aacb0b3d82a7b4edc3caa899184b # releases/CGAL-5.0
    # For whatever reason, this keeps downloading forever (repeats downloads if finished)
    URL      https://github.com/CGAL/cgal/archive/refs/tags/v5.4.zip
    URL_HASH SHA256=d7605e0a5a5ca17da7547592f6f6e4a59430a0bc861948974254d0de43eab4c0
    DEPENDS dep_Boost dep_GMP dep_MPFR
    # Clang 16+/21 instantiates a broken safe-bool operator in CGAL's halfedge
    # iterators (this->base() with no base class). Patch it to match the correct
    # sibling iterators in the same header.
    PATCH_COMMAND ${CMAKE_COMMAND} -DCGAL_ITERATOR_H=<SOURCE_DIR>/include/CGAL/boost/graph/iterator.h -P ${CMAKE_CURRENT_LIST_DIR}/fix-cgal-iterator-base.cmake
)

include(GNUInstallDirs)
