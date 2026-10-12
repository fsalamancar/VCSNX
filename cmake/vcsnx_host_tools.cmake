# Mac-only tools. vcs_recomp is the profile-specific generator used by tools/regen_generated.sh.
add_executable(vcs_recomp "${VCSNX_PROFILE_DIR}/tools/vcs_codegen_main.cpp")
target_link_libraries(vcs_recomp PRIVATE psprecomp_core)
