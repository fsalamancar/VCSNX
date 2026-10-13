# hbmenu/Sphaira packaging (spec §5): NACP metadata (what Sphaira's forwarder shows) and the .nro.
nx_generate_nacp(vcsnx.nacp
    NAME "GTA Vice City Stories"
    AUTHOR "VCSNX (PSPRecomp port)"
    VERSION "${PROJECT_VERSION}")
# The icon is cut from the user's own cover art (tools/make_icon.py) and lives in the git-ignored
# local/ folder; without it the .nro gets libnx's default icon. A release is built with
# -DVCSNX_LOCAL_ICON=OFF: the published runtime must not carry the game's art (tools/package_builder.py).
option(VCSNX_LOCAL_ICON "Use local/icon.jpg as the .nro icon (development builds only)" ON)
set(VCSNX_ICON "${CMAKE_SOURCE_DIR}/local/icon.jpg")
if(VCSNX_LOCAL_ICON AND EXISTS "${VCSNX_ICON}")
    message(STATUS "VCSNX: .nro icon ${VCSNX_ICON}")
    nx_create_nro(vcsnx NACP vcsnx.nacp ICON "${VCSNX_ICON}")
else()
    message(STATUS "VCSNX: libnx's default .nro icon (VCSNX_LOCAL_ICON=OFF or no local/icon.jpg, see tools/make_icon.py)")
    nx_create_nro(vcsnx NACP vcsnx.nacp)
endif()
