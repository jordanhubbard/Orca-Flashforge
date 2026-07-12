# Replaces the include of the removed Classic-Mac-OS <fp.h> header in OpenCV's
# bundled libpng with <math.h>. Modern macOS SDKs define TARGET_OS_MAC=1, which
# selects a legacy code path in pngpriv.h that no longer compiles.
file(READ "${PNGPRIV}" _contents)
string(REPLACE "#      include <fp.h>" "#      include <math.h>" _contents "${_contents}")
file(WRITE "${PNGPRIV}" "${_contents}")
message(STATUS "Patched libpng pngpriv.h to use <math.h> instead of <fp.h>")
