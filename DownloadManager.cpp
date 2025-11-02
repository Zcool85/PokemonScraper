//
// Created by Zéro Cool on 01/11/2025.
//

#include "DownloadManager.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <curl/curl.h>

bool createDirectoriesForFile(const std::string& filepath) {
    try {
        std::filesystem::path p(filepath);
        std::filesystem::path directory = p.parent_path();

        if (!directory.empty() && !std::filesystem::exists(directory)) {
            std::filesystem::create_directories(directory);
        }
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Erreur lors de la création des répertoires: "
                  << e.what() << std::endl;
        return false;
    }
}


size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
    size_t totalSize = size * nmemb;
    buffer->append((char*)contents, totalSize);
    return totalSize;
}

size_t WriteCallbackFile(void* contents, size_t size, size_t nmemb, FILE* file) {
    return fwrite(contents, size, nmemb, file);
}

auto DownloadManager::Initialize() -> bool
{
    const auto result = curl_global_init(CURL_GLOBAL_DEFAULT);
    return result == CURLE_OK;
}

auto DownloadManager::downloadJson(const std::string& url, std::string& json_data, std::string& etag,
    std::string& last_update, bool& has_changed) -> bool
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Init curl error" << std::endl;
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 sec
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "It's me, Mario/1.0");

    curl_slist *list = nullptr;
    list = curl_slist_append(list, "accept: application/json");

    if (!etag.empty()) {
        const std::string ifNoneMatch = "If-None-Match: " + etag;
        list = curl_slist_append(list, ifNoneMatch.c_str());
    }

    if (!last_update.empty()) {
        const std::string ifModifiedSince = "If-Modified-Since: " + last_update;
        list = curl_slist_append(list, ifModifiedSince.c_str());
    }

    if (list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    }

    if (const CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        return false;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode == 304) {
        has_changed = false;
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        return true;
    }

    if (httpCode != 200) {
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        std::cerr << "HTTP Error: " << httpCode << std::endl;
        return false;
    }

    has_changed = true;

    etag.clear();
    last_update.clear();

    curl_header *etagHeader = nullptr;
    if (const CURLHcode res = curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &etagHeader); res == CURLHE_OK)
    {
        etag.append(etagHeader->value);
    }
    curl_header *lastModifiedHeader = nullptr;
    if (const CURLHcode res = curl_easy_header(curl, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader); res == CURLHE_OK)
    {
        last_update.append(lastModifiedHeader->value);
    }

    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    return true;
}

auto DownloadManager::downloadFile(const std::string& url, const std::string& destination, std::string& etag,
    std::string& last_update, bool& has_changed) -> bool
{
    if (!createDirectoriesForFile(destination))
    {
        return false;
    }

    FILE *file = fopen(destination.c_str(), "wb");

    if (!file) {
        std::cerr << "ERROR opening file " << destination << std::endl;
        return false;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Init curl error" << std::endl;
        fclose(file);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 sec
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "It's me, Mario/1.0");

    curl_slist *list = nullptr;

    if (!etag.empty()) {
        const std::string ifNoneMatch = "If-None-Match: " + etag;
        list = curl_slist_append(list, ifNoneMatch.c_str());
    }

    if (!last_update.empty()) {
        const std::string ifModifiedSince = "If-Modified-Since: " + last_update;
        list = curl_slist_append(list, ifModifiedSince.c_str());
    }

    if (list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    }

    if (const CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
        fclose(file);
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(file);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode == 304) {
        has_changed = false;
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        return true;
    }

    if (httpCode != 200) {
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        std::cerr << "HTTP Error: " << httpCode << std::endl;
        return false;
    }

    has_changed = true;

    etag.clear();
    last_update.clear();

    curl_header *etagHeader = nullptr;
    if (const CURLHcode res = curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &etagHeader); res == CURLHE_OK)
    {
        etag.append(etagHeader->value);
    }
    curl_header *lastModifiedHeader = nullptr;
    if (const CURLHcode res = curl_easy_header(curl, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader); res == CURLHE_OK)
    {
        last_update.append(lastModifiedHeader->value);
    }

    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    return true;
}
