# ---------------------------------------------------------------------------
# Author:      Jon Gough Copyright:   2020 License:     GPLv3+
# ---------------------------------------------------------------------------

# This file contains changes needed during the make package process depending on the type of package being created

if(CPACK_GENERATOR MATCHES "DEB")
    set(CPACK_PACKAGE_FILE_NAME "celestial_navigation_pi-2.4.43.2-msvc-x86-10.0.26100")
    if(CPACK_DEBIAN_PACKAGE_ARCHITECTURE MATCHES "x86_64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE )
    endif()
else()
    set(CPACK_PACKAGE_FILE_NAME "celestial_navigation_pi-2.4.43.2-msvc-x86-10.0.26100")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE x86)
endif()
