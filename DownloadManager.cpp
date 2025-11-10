//
// Created by Zéro Cool on 01/11/2025.
//

#include "DownloadManager.h"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <curl/curl.h>

#include "Logs.h"

bool DownloadManager::m_initialized = false;

struct transfer_private_data {
    size_t download_index = 0;
    std::ofstream* file = nullptr;
    std::string file_path;
    curl_slist* list = nullptr;
};

size_t WriteCallback(void* contents, const size_t size, const size_t nmemb, const transfer_private_data* transfer) {
    if (!transfer->file->is_open())
    {
        // We create the path if needed
        if (const auto parent_path = std::filesystem::path(transfer->file_path).parent_path(); !std::filesystem::exists(parent_path))
        {
            std::filesystem::create_directories(parent_path);
        }

        transfer->file->open(transfer->file_path, std::ios::out | std::ios::trunc | std::ios::binary);
    }

    const size_t totalSize = size * nmemb;
    transfer->file->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

auto DownloadManager::download(std::vector<DownloadParameter>& download_parameters) const -> std::vector<DownloadResult>
{
    initialize();

    // Initialize empty result
    auto result = std::vector<DownloadResult>(download_parameters.size());

    // Initialize multi download
    const auto multi_handle = curl_multi_init();
    if (!multi_handle)
    {
        CURL_ERROR("curl_multi_init");
    }

    // Configurer le multiplexing HTTP/2
    curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);

    size_t batch_size = 0;
    size_t i = 0;

    while (i < download_parameters.size())
    {
        CURL_TRACE("{}/{} ({} %)", i, download_parameters.size(), static_cast<float>(i) / static_cast<float>(download_parameters.size()) * 100.0f);

        // Add download to multi download
        while (batch_size < m_max_parallel && i < download_parameters.size())
        {
            std::string etag;
            std::string last_update;

            // We get the current download
            result[i].parameter = &download_parameters[i];

            if (std::filesystem::exists(result[i].parameter->destination_file_path))
            {
                // If the file exists, we get metadata
                if (auto uri_metadata = m_database_manager.getUriMetadata(result[i].parameter->uri); uri_metadata.has_value())
                {
                    etag = uri_metadata->etag;
                    last_update = uri_metadata->last_update;
                }
            }

            // Prepare private data
            auto private_data = new transfer_private_data();
            private_data->download_index = i;
            private_data->file = new std::ofstream();
            private_data->file_path = result[i].parameter->destination_file_path;

            // We prepare curl download for this file
            CURL* curl_easy_handle = curl_easy_init();
            if (!curl_easy_handle)
            {
                CURL_ERROR("curl_easy_init for {}", result[i].parameter->uri);
            }

            // Configuration HTTP/2
            curl_easy_setopt(curl_easy_handle, CURLOPT_URL, download_parameters[i].uri.c_str());

            // Callback d'écriture
            curl_easy_setopt(curl_easy_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl_easy_handle, CURLOPT_WRITEDATA, private_data);
            curl_easy_setopt(curl_easy_handle, CURLOPT_PRIVATE, private_data);

            curl_easy_setopt(curl_easy_handle, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl_easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl_easy_handle, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl_easy_handle, CURLOPT_NOSIGNAL, 1L);

            curl_easy_setopt(curl_easy_handle, CURLOPT_USERAGENT, "It's me, Mario/1.0");

            /* enlarge the receive buffer for potentially higher transfer speeds */
            curl_easy_setopt(curl_easy_handle, CURLOPT_BUFFERSIZE, 100000L);

            /* HTTP/2 please */
            curl_easy_setopt(curl_easy_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

#if (CURLPIPE_MULTIPLEX > 0)
            /* wait for pipe connection to confirm */
            // Activer le multiplexing (réutilisation de connexion)
            curl_easy_setopt(curl_easy_handle, CURLOPT_PIPEWAIT, 1L);
#endif

            private_data->list = nullptr;
            private_data->list = curl_slist_append(private_data->list, "accept: application/json");

            if (!etag.empty()) {
                const std::string ifNoneMatch = "If-None-Match: " + etag;
                private_data->list = curl_slist_append(private_data->list, ifNoneMatch.c_str());
            }

            if (!last_update.empty()) {
                const std::string ifModifiedSince = "If-Modified-Since: " + last_update;
                private_data->list = curl_slist_append(private_data->list, ifModifiedSince.c_str());
            }

            if (private_data->list) {
                curl_easy_setopt(curl_easy_handle, CURLOPT_HTTPHEADER, private_data->list);
            }

            curl_multi_add_handle(multi_handle, curl_easy_handle);

            batch_size++;
            i++;
        }

        // Downloads items
        int still_running = 0;
        curl_multi_perform(multi_handle, &still_running);

        while (still_running) {
            int num_file_descriptors = 0;

            if (auto mc = curl_multi_wait(multi_handle, nullptr, 0, 100, &num_file_descriptors); mc != CURLM_OK) {
                CURL_ERROR("Erreur curl_multi_wait: {}", curl_multi_strerror(mc));
                break;
            }

            curl_multi_perform(multi_handle, &still_running);
        }

        // We get results
        CURLMsg* msg;
        int msgs_left;
        while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
            // We wait exclusively for ended downloads
            if (msg->msg != CURLMSG_DONE)
            {
                continue;
            }

            CURL* eh = msg->easy_handle;

            // We remove this handle
            curl_multi_remove_handle(multi_handle, eh);
            batch_size--;

            char* url;
            transfer_private_data* private_data;

            curl_easy_getinfo(eh, CURLINFO_EFFECTIVE_URL, &url);
            curl_easy_getinfo(eh, CURLINFO_PRIVATE, &private_data);

            auto download_index = private_data->download_index;

            // Free all memories
            if (private_data->file->is_open())
            {
                private_data->file->close();
            }
            if (private_data->list) {
                curl_slist_free_all(private_data->list);
            }
            delete private_data;

            result[download_index].effective_url = url;

            // Check if any curl error
            if (CURLcode data_result = msg->data.result; data_result != CURLE_OK)
            {
                // TODO : Get Headers ?
                CURL_ERROR("Download error for {}: {}", url, curl_easy_strerror(data_result));

                result[download_index].success = false;
                result[download_index].error = curl_easy_strerror(data_result);

                continue;
            }

            long httpCode = 0;
            curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &httpCode);

            if (httpCode != 304 && httpCode != 200)
            {
                // TODO : Get headers ?
                CURL_ERROR("Download error for {}, status code {}", url, httpCode);

                result[download_index].success = false;
                result[download_index].error = fmt::format("HTTP status {}", httpCode);

                // cleanup
                curl_easy_cleanup(eh);

                continue;
            }

            // Get HTTP version
            long http_version;
            curl_easy_getinfo(eh, CURLINFO_HTTP_VERSION, &http_version);

            if (httpCode == 304) {
                if (http_version == CURL_HTTP_VERSION_2_0) {
                    CURL_INFO("No change (HTTP/2) for {}", url);
                }
                else
                {
                    CURL_INFO("No change for {}", url);
                }

                result[download_index].success = true;

                // cleanup
                curl_easy_cleanup(eh);

                continue;
            }

            if (httpCode == 200) {
                std::string etag;
                std::string last_update;

                curl_header *etagHeader = nullptr;
                if (const CURLHcode result_code = curl_easy_header(eh, "etag", 0, CURLH_HEADER, -1, &etagHeader); result_code == CURLHE_OK)
                {
                    etag.append(etagHeader->value);
                }
                curl_header *lastModifiedHeader = nullptr;
                if (const CURLHcode result_code = curl_easy_header(eh, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader); result_code == CURLHE_OK)
                {
                    last_update.append(lastModifiedHeader->value);
                }

                if (!m_database_manager.upsertUriMetadata({url, etag, last_update}))
                {
                    CURL_ERROR("Erreur upsertUriMetadata: {}", url);
                }

                if (http_version == CURL_HTTP_VERSION_2_0) {
                    CURL_INFO("Successfully downloaded (HTTP/2) {}", url);
                }
                else
                {
                    CURL_INFO("Successfully downloaded {}", url);
                }

                result[download_index].success = true;
                result[download_index].has_changed = true;

                // cleanup
                curl_easy_cleanup(eh);
            }
        }
    }

    if (curl_multi_cleanup(multi_handle) != CURLM_OK)
    {
        CURL_ERROR("curl_multi_cleanup");
    }

    return result;
}

auto DownloadManager::initialize() -> void
{
    if (!m_initialized)
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        {
            CURL_ERROR("CURL global init error");
            return;
        }

        m_initialized = true;
    }
}

DownloadManager::DownloadManager(DatabaseManager& database_manager, const size_t max_parallel)
    : m_database_manager(database_manager), m_max_parallel(max_parallel)
{
    initialize();

    // m_multi_handle = curl_multi_init();
    // curl_multi_setopt(m_multi_handle, CURLMOPT_MAXCONNECTS, max_parallel);
}

// DownloadManager::~DownloadManager()
// {
//     // if (m_multi_handle)
//     // {
//     //     curl_multi_cleanup(m_multi_handle);
//     // }
//     //
//     // m_multi_handle = nullptr;
// }

// auto DownloadManager::downloadJson(const std::string& url, std::string& json_data, std::string& etag,
//                                    std::string& last_update, bool& has_changed) -> bool
// {
//     CURL *curl = curl_easy_init();
//     if (!curl) {
//         std::cerr << "Init curl error" << std::endl;
//         return false;
//     }
//
//     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_data);
//     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
//     curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
//     curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 sec
//     curl_easy_setopt(curl, CURLOPT_USERAGENT, "It's me, Mario/1.0");
//
//     curl_slist *list = nullptr;
//     list = curl_slist_append(list, "accept: application/json");
//
//     if (!etag.empty()) {
//         const std::string ifNoneMatch = "If-None-Match: " + etag;
//         list = curl_slist_append(list, ifNoneMatch.c_str());
//     }
//
//     if (!last_update.empty()) {
//         const std::string ifModifiedSince = "If-Modified-Since: " + last_update;
//         list = curl_slist_append(list, ifModifiedSince.c_str());
//     }
//
//     if (list) {
//         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
//     }
//
//     if (const CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
//         std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         return false;
//     }
//
//     long httpCode = 0;
//     curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
//
//     if (httpCode == 304) {
//         has_changed = false;
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         return true;
//     }
//
//     if (httpCode != 200) {
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         std::cerr << "HTTP Error: " << httpCode << std::endl;
//         return false;
//     }
//
//     has_changed = true;
//
//     etag.clear();
//     last_update.clear();
//
//     curl_header *etagHeader = nullptr;
//     if (const CURLHcode res = curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &etagHeader); res == CURLHE_OK)
//     {
//         etag.append(etagHeader->value);
//     }
//     curl_header *lastModifiedHeader = nullptr;
//     if (const CURLHcode res = curl_easy_header(curl, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader); res == CURLHE_OK)
//     {
//         last_update.append(lastModifiedHeader->value);
//     }
//
//     curl_slist_free_all(list);
//     curl_easy_cleanup(curl);
//
//     return true;
// }
//
// auto DownloadManager::downloadFile(const std::string& url, const std::string& destination, std::string& etag,
//     std::string& last_update, bool& has_changed) -> bool
// {
//     if (!createDirectoriesForFile(destination))
//     {
//         return false;
//     }
//
//     auto add_headers = std::filesystem::exists(destination);
//
//     FILE *file = fopen(destination.c_str(), "wb");
//
//     if (!file) {
//         std::cerr << "ERROR opening file " << destination << std::endl;
//         return false;
//     }
//
//     CURL *curl = curl_easy_init();
//     if (!curl) {
//         std::cerr << "Init curl error" << std::endl;
//         fclose(file);
//         return false;
//     }
//
//     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackFile);
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
//     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
//     curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
//     curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 sec
//     curl_easy_setopt(curl, CURLOPT_USERAGENT, "It's me, Mario/1.0");
//
//     curl_slist *list = nullptr;
//
//     if (add_headers)
//     {
//         if (!etag.empty()) {
//             const std::string ifNoneMatch = "If-None-Match: " + etag;
//             list = curl_slist_append(list, ifNoneMatch.c_str());
//         }
//
//         if (!last_update.empty()) {
//             const std::string ifModifiedSince = "If-Modified-Since: " + last_update;
//             list = curl_slist_append(list, ifModifiedSince.c_str());
//         }
//     }
//
//     if (list) {
//         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
//     }
//
//     if (const CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
//         std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
//         fclose(file);
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         return false;
//     }
//
//     fclose(file);
//
//     long httpCode = 0;
//     curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
//
//     if (httpCode == 304) {
//         has_changed = false;
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         return true;
//     }
//
//     if (httpCode != 200) {
//         curl_slist_free_all(list);
//         curl_easy_cleanup(curl);
//         std::cerr << "HTTP Error: " << httpCode << std::endl;
//         return false;
//     }
//
//     has_changed = true;
//
//     etag.clear();
//     last_update.clear();
//
//     curl_header *etagHeader = nullptr;
//     if (const CURLHcode res = curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &etagHeader); res == CURLHE_OK)
//     {
//         etag.append(etagHeader->value);
//     }
//     curl_header *lastModifiedHeader = nullptr;
//     if (const CURLHcode res = curl_easy_header(curl, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader); res == CURLHE_OK)
//     {
//         last_update.append(lastModifiedHeader->value);
//     }
//
//     curl_slist_free_all(list);
//     curl_easy_cleanup(curl);
//
//     return true;
// }
