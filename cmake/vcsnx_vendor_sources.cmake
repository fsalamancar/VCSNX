# Vendor host sources used by VCSNX. Left out: the fork's windows (platform/*/display_window*),
# which platform/switch replaces, and ge_gpu_backend_dx12.cpp. The GE GPU backend sources are
# chosen by cmake/vcsnx_vulkan.cmake.
set(VCSNX_VENDOR_HOST_SOURCES
    "${VCSNX_PROFILE_DIR}/host/vcs_bootstrap_paths.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_profile.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_native_fast_paths.cpp"
    "${VCSNX_PROFILE_DIR}/host/framebuffer_capture.cpp"
    "${VCSNX_PROFILE_DIR}/host/audio_output.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_config.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_texture_replace.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_key_prompts.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_camera_input.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_vehicle_input.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_fps_overlay.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_media_decoder.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_project2dfx.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_project2dfx_lights.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_draw_distance_patch.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_runtime_log.cpp"
    "${VCSNX_PROFILE_DIR}/host/vcs_hdr_post_dx12_stub.cpp"
    "${VCSNX_PROFILE_DIR}/host/ge_renderer.cpp"
    "${VCSNX_PROFILE_DIR}/host/dx12_presenter.cpp")
