# CGAL 5.4's boost/graph/iterator.h has a latent bug in the "safe bool" conversion
# operator of Halfedge_around_{source,target,face}_iterator: it calls this->base(),
# but those classes have no base class and no base() member. Older Clang never
# instantiated the method; Clang 16+/21 does, turning it into a hard error. The
# sibling iterators in the same header already use the correct `!(g == nullptr)`
# check, so mirror that.
set(_hdr "${CGAL_ITERATOR_H}")
file(READ "${_hdr}" _contents)
string(REPLACE "(! (this->base() == nullptr))" "(! (g == nullptr))" _contents "${_contents}")
file(WRITE "${_hdr}" "${_contents}")
message(STATUS "Patched CGAL boost/graph/iterator.h safe-bool operator (this->base -> g)")
