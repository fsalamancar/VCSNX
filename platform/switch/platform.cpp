#include "vcsnx_platform.hpp"

#include "stderr_tee.hpp"

#include <SDL.h>

namespace vcsnx {

std::string data_directory() {
#if defined(VCSNX_DATA_DIR)
    return VCSNX_DATA_DIR;
#else
    if (char *base = SDL_GetBasePath()) {
        std::string directory(base);
        SDL_free(base);
        if (!directory.empty() && directory.back() == '/') directory.pop_back();
        return directory;
    }
    return ".";
#endif
}

void platform_startup() {
    install_stderr_tee(data_directory());
}

} // namespace vcsnx
