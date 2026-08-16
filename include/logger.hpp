#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Logger
{
    inline void Init(const char* filepath, const bool truncate = false)
    {
        // Check if already exists
        if (!spdlog::get("root")) {
            spdlog::set_default_logger(
                spdlog::basic_logger_mt("root", filepath, truncate)
            );
        }

        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] %l: %v");
    }
}
