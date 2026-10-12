# Pinned sources for VCSNX (spec §2.1). Sourced by tools/import_vendor.sh and tools/fetch_upstream.sh.
FORK_URL=https://github.com/OverkillLabs3/vice-city-stories-recomp.git
FORK_COMMIT=b5a861d200ad66a126d51ec2fcef56b0455e4daf
UPSTREAM_URL=https://github.com/jessicanataliagta/PSPRecomp.git
UPSTREAM_COMMIT=f6e7d415c7f447b934cc3865a31eb725f353d659

# fetch_pinned <url> <commit> <dest>: shallow-fetches exactly <commit> into a fresh <dest>.
fetch_pinned() {
    local url="$1" commit="$2" dest="$3"
    rm -rf "$dest"
    git init -q "$dest"
    git -C "$dest" remote add origin "$url"
    git -C "$dest" fetch -q --depth 1 origin "$commit"
    git -C "$dest" checkout -q FETCH_HEAD
    if [ "$(git -C "$dest" rev-parse HEAD)" != "$commit" ]; then
        echo "fetched commit does not match $commit" >&2
        return 1
    fi
}

# mesa-switch: Mesa 26.2.4 with NVK (Vulkan) for the Switch, built as static libnx archives.
MESA_SWITCH_URL=https://github.com/danfromtico/mesa-switch.git
MESA_SWITCH_COMMIT=606441c90e459028a50fca352daebfc20211f890
