//
// Created by Zéro Cool on 03/11/2025.
//

#include <spdlog/sinks/stdout_color_sinks-inl.h>

#include "Logs.h"

std::shared_ptr<spdlog::logger> Logs::s_app_logger;
std::shared_ptr<spdlog::logger> Logs::s_db_logger;
std::shared_ptr<spdlog::logger> Logs::s_curl_logger;

void Logs::Initialize()
{
    spdlog::set_pattern("%^[%T] %n: %v%$");
    s_app_logger = spdlog::stdout_color_mt("APP");
    s_app_logger->set_level(spdlog::level::trace);
    s_db_logger = spdlog::stdout_color_mt("DB");
    s_db_logger->set_level(spdlog::level::trace);
    s_curl_logger = spdlog::stdout_color_mt("CURL");
    s_curl_logger->set_level(spdlog::level::trace);
}
