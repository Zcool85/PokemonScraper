//
// Created by Zéro Cool on 01/11/2025.
//

#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <vector>
#include <string>

#include "DatabaseManager.h"

class DownloadManager {
    static bool m_initialized;
    DatabaseManager& m_database_manager;
    size_t m_max_parallel;
public:
    explicit DownloadManager(DatabaseManager& database_manager, size_t max_parallel = 50);

    struct DownloadParameter {
        std::string uri;
        std::string destination_file_path;
    };

    struct DownloadResult {
        DownloadParameter* parameter;
        std::string effective_url;
        bool success;
        std::string error;
        bool has_changed = false;
    };

    [[nodiscard]] auto download(std::vector<DownloadParameter>& download_parameters) const -> std::vector<DownloadResult>;
private:
    static auto initialize() -> void;
};

#endif //DOWNLOAD_MANAGER_H
