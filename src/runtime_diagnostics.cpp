#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <exception>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void report_abort(int) {
    std::fputs("SpaceRanger fatal: abort; native stack follows.\n", stderr);
#ifdef _WIN32
    void* frames[48]{};
    const auto count = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (unsigned i = 0; i < count; ++i) {
        HMODULE module{};
        char path[MAX_PATH]{};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(frames[i]), &module);
        GetModuleFileNameA(module, path, MAX_PATH);
        std::fprintf(stderr, "  %s + 0x%llX\n", path,
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(frames[i]) -
                reinterpret_cast<uintptr_t>(module)));
    }
#endif
    std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}
}

extern "C" void ts2_install_runtime_diagnostics() {
    std::signal(SIGABRT, report_abort);
    std::set_terminate([] {
        if (auto exception = std::current_exception()) {
            try { std::rethrow_exception(exception); }
            catch (const std::exception& e) {
                std::fprintf(stderr, "SpaceRanger uncaught exception: %s\n", e.what());
            }
            catch (...) { std::fputs("SpaceRanger uncaught non-standard exception\n", stderr); }
        }
        std::abort();
    });
}
