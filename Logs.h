//
// Created by Zéro Cool on 03/11/2025.
//

#ifndef LOGS_H
#define LOGS_H

#include <memory>
#include <spdlog/spdlog.h>

class Logs {
public:
    Logs() = delete;
    Logs(const Logs&) = delete;
    Logs &operator=(const Logs&) = delete;

    static void Initialize();

    static std::shared_ptr<spdlog::logger> &GetAppLogger()
    {
        return s_app_logger;
    }
    static std::shared_ptr<spdlog::logger> &GetDbLogger()
    {
        return s_db_logger;
    }
    static std::shared_ptr<spdlog::logger> &GetCurlLogger()
    {
        return s_curl_logger;
    }

private:
    static std::shared_ptr<spdlog::logger> s_app_logger;
    static std::shared_ptr<spdlog::logger> s_db_logger;
    static std::shared_ptr<spdlog::logger> s_curl_logger;
};

#define APP_TRACE(...)         ::Logs::GetAppLogger()->trace(__VA_ARGS__)
#define APP_DEBUG(...)         ::Logs::GetAppLogger()->debug(__VA_ARGS__)
#define APP_INFO(...)          ::Logs::GetAppLogger()->info(__VA_ARGS__)
#define APP_WARN(...)          ::Logs::GetAppLogger()->warn(__VA_ARGS__)
#define APP_ERROR(...)         ::Logs::GetAppLogger()->error(__VA_ARGS__)
#define APP_CRITICAL(...)      ::Logs::GetAppLogger()->critical(__VA_ARGS__)

#define DB_TRACE(...)          ::Logs::GetDbLogger()->trace(__VA_ARGS__)
#define DB_DEBUG(...)          ::Logs::GetDbLogger()->debug(__VA_ARGS__)
#define DB_INFO(...)           ::Logs::GetDbLogger()->info(__VA_ARGS__)
#define DB_WARN(...)           ::Logs::GetDbLogger()->warn(__VA_ARGS__)
#define DB_ERROR(...)          ::Logs::GetDbLogger()->error(__VA_ARGS__)
#define DB_CRITICAL(...)       ::Logs::GetDbLogger()->critical(__VA_ARGS__)

#define CURL_TRACE(...)          ::Logs::GetCurlLogger()->trace(__VA_ARGS__)
#define CURL_DEBUG(...)          ::Logs::GetCurlLogger()->debug(__VA_ARGS__)
#define CURL_INFO(...)           ::Logs::GetCurlLogger()->info(__VA_ARGS__)
#define CURL_WARN(...)           ::Logs::GetCurlLogger()->warn(__VA_ARGS__)
#define CURL_ERROR(...)          ::Logs::GetCurlLogger()->error(__VA_ARGS__)
#define CURL_CRITICAL(...)       ::Logs::GetCurlLogger()->critical(__VA_ARGS__)

#endif //LOGS_H
