//
// Created by Zéro Cool on 01/11/2025.
//

#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H
#include <string>

class DownloadManager {
public:
    static auto Initialize() -> bool;
    static auto downloadJson(const std::string& url, std::string& json_data, std::string& etag,
                             std::string& last_update, bool& has_changed) -> bool;
    static auto downloadFile(const std::string& url, const std::string& destination, std::string& etag,
                             std::string& last_update, bool& has_changed) -> bool;
};

#endif //DOWNLOAD_MANAGER_H
