# hbmenu/Sphaira packaging (spec §5): NACP metadata (what Sphaira's forwarder shows) and the .nro.
nx_generate_nacp(vcsnx.nacp
    NAME "GTA Vice City Stories"
    AUTHOR "VCSNX (PSPRecomp port)"
    VERSION "${PROJECT_VERSION}")
# The icon is cut from the user's own cover art (tools/make_icon.py) and lives in the git-ignored
# local/ folder; without it the .nro gets libnx's default icon.
set(VCSNX_ICON "${CMAKE_SOURCE_DIR}/local/icon.jpg")
if(EXISTS "${VCSNX_ICON}")
    message(STATUS "VCSNX: .nro icon ${VCSNX_ICON}")
    nx_create_nro(vcsnx NACP vcsnx.nacp ICON "${VCSNX_ICON}")
else()
    message(STATUS "VCSNX: no local/icon.jpg, using libnx's default .nro icon (see tools/make_icon.py)")
    nx_create_nro(vcsnx NACP vcsnx.nacp)
endif()
