#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
#include <unordered_map>
#include <ranges>
#include <string>
#include <vector>
#include <fmt/format.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/error/en.h>

#include "Logs.h"
#include "DatabaseManager.h"
#include "DownloadManager.h"

std::string sanitizeForPath(std::string filename) {
    static const std::unordered_map<unsigned char, char> replacements = {
        {'<', '('},  {'>', ')'},  {':', '-'},
        {'"', '\''}, {'/', '-'},  {'\\', '-'},
        {'|', '-'},  {'?', ' '},  {'*', '+'}
    };

    // Traiter octet par octet, mais préserver les séquences UTF-8
    for (size_t i = 0; i < filename.size(); ) {
        // Ne remplacer que les caractères ASCII interdits
        if (const unsigned char c = filename[i]; c < 128) {
            if (auto it = replacements.find(c); it != replacements.end()) {
                filename[i] = it->second;
            } else if (c < 32) {
                filename[i] = '_';
            }

            i++;
        }
        // Sauter les séquences UTF-8 multi-octets
        else if (c >= 128) {
            if ((c & 0xE0) == 0xC0) i += 2;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else if ((c & 0xF8) == 0xF0) i += 4;
            else i++; // Octet invalide
        } else {
            i++;
        }
    }

    return filename;
}

std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (const char c : value) {
        // Garde alphanumérique et quelques caractères spéciaux intacts
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            } else {
                // Encode les autres caractères
                escaped << std::uppercase;
                escaped << '%' << std::setw(2)
                        << static_cast<int>(static_cast<unsigned char>(c));
                escaped << std::nouppercase;
            }
    }

    return escaped.str();
}

auto refreshAllSets(const DownloadManager& download_manager, const std::map<std::string, std::string>& languages) -> void
{
    APP_INFO("Refreshing all sets...");

    std::vector<DownloadManager::DownloadParameter> parameters;
    for (const auto& lang_id: languages | std::views::keys)
    {
        parameters.push_back(DownloadManager::DownloadParameter {
            fmt::format("https://api.tcgdex.net/v2/{0}/sets", urlEncode(lang_id)),
            fmt::format("data/{0}/sets.json", lang_id)}
            );
    }

    for (const auto results = download_manager.download(parameters); const auto& result : results)
    {
        if (result.success)
        {
            APP_TRACE("{} -> Success ({})",
                result.effective_url,
                result.has_changed ? "Has changed" : "no changes");
        }
        else
        {
            APP_TRACE("{} -> ERROR: {}",
                result.effective_url,
                result.error);
        }
    }
}

auto refreshAllCards(const DownloadManager& download_manager) -> void
{
    APP_INFO("Refreshing all cards...");

    std::vector<DownloadManager::DownloadParameter> parameters;

    for (const auto data_path = std::filesystem::path("data"); const auto& language_directory : std::filesystem::directory_iterator(data_path))
    {
        auto lang_id = language_directory.path().filename().string();

        auto json_set_path = language_directory.path() / "sets.json";

        if (!std::filesystem::exists(json_set_path))
        {
            APP_INFO("{} does not exist", json_set_path.string());
            continue;
        }

        APP_TRACE("{}: Read json file for lang id {}...", json_set_path.string(), lang_id);

        std::ifstream ifs(json_set_path.string());
        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;

        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            APP_ERROR("{}: Does not have valid JSON, removing file !", json_set_path.string());
            APP_ERROR("  Reason: {}", rapidjson::GetParseError_En(doc.GetParseError()));
            APP_ERROR("  At: {}", doc.GetErrorOffset());
            APP_ERROR("  -> Removing file !", json_set_path.string());

            ifs.close();

            std::filesystem::remove(json_set_path);

            continue;
        }

        if (!doc.IsArray())
        {
            APP_ERROR("{}: Root is not an array, removing file !", json_set_path.string());

            ifs.close();

            std::filesystem::remove(json_set_path);

            continue;
        }

        APP_TRACE("{}: Have {} sets", json_set_path.string(), doc.Size());

        for (const auto& set: doc.GetArray())
        {
            if (!set.IsObject() || !set.HasMember("id") || !set["id"].IsString())
            {
                APP_ERROR("{}: Invalid set format, removing file !", json_set_path.string());

                ifs.close();

                std::filesystem::remove(json_set_path);

                continue;
            }

            std::string set_id = set["id"].GetString();

            APP_TRACE("{}: set id {}", json_set_path.string(), set_id);

            parameters.push_back(DownloadManager::DownloadParameter {
                        fmt::format("https://api.tcgdex.net/v2/{0}/sets/{1}", urlEncode(lang_id), urlEncode(set_id)),
                        fmt::format("data/{0}/{1}/cards.json", lang_id, set_id)}
                        );
        }
    }

    for (const auto results = download_manager.download(parameters); const auto& result : results)
    {
        if (result.success)
        {
            APP_TRACE("{} -> Success ({})",
                result.effective_url,
                result.has_changed ? "Has changed" : "no changes");
        }
        else
        {
            APP_TRACE("{} -> ERROR: {}",
                result.effective_url,
                result.error);
        }
    }
}

auto downloadCards(const DownloadManager& download_manager) -> void
{
    APP_INFO("Refreshing all cards...");

    std::vector<DownloadManager::DownloadParameter> parameters;

    for (const auto data_path = std::filesystem::path("data"); const auto& language_directory : std::filesystem::directory_iterator(data_path))
    {
        auto lang_id = language_directory.path().filename().string();

        for (const auto& set_directory : std::filesystem::directory_iterator(language_directory))
        {
            auto set_id = set_directory.path().filename().string();

            auto json_cards_path = set_directory.path() / "cards.json";

            if (!std::filesystem::exists(json_cards_path))
            {
                APP_INFO("{} does not exist", json_cards_path.string());
                continue;
            }

            APP_TRACE("{}: Read json file for lang id {} and set id {}...", json_cards_path.string(), lang_id, set_id);

            std::ifstream ifs(json_cards_path.string());
            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;

            doc.ParseStream(isw);

            if (doc.HasParseError()) {
                APP_ERROR("{}: Does not have valid JSON, removing file !", json_cards_path.string());
                APP_ERROR("  Reason: {}", rapidjson::GetParseError_En(doc.GetParseError()));
                APP_ERROR("  At: {}", doc.GetErrorOffset());
                APP_ERROR("  -> Removing file !", json_cards_path.string());

                ifs.close();

                std::filesystem::remove(json_cards_path);

                continue;
            }

            if (!doc.IsObject())
            {
                APP_ERROR("{}: Root is not an object, removing file !", json_cards_path.string());

                ifs.close();

                std::filesystem::remove(json_cards_path);

                continue;
            }

            if (!doc.HasMember("cards") || !doc["cards"].IsArray())
            {
                APP_ERROR("{}: No cards members found, removing file !", json_cards_path.string());

                ifs.close();

                std::filesystem::remove(json_cards_path);

                continue;
            }

            auto cards = doc["cards"].GetArray();

            APP_TRACE("{}: Have {} cards", json_cards_path.string(), cards.Size());

            size_t card_index = 0;
            for (const auto& card: cards)
            {
                card_index++;

                if (!card.IsObject() || !card.HasMember("localId") || !card["localId"].IsString())
                {
                    APP_ERROR("{}: No localId card definition for card index {}", json_cards_path.string(), card_index);
                    continue;
                }

                std::string local_id = card["localId"].GetString();

                if (!card.IsObject() || !card.HasMember("name") || !card["name"].IsString())
                {
                    APP_ERROR("{}: No name card definition for card index {}", json_cards_path.string(), card_index);
                    continue;
                }

                std::string name = card["name"].GetString();

                if (!card.IsObject() || !card.HasMember("image") || !card["image"].IsString())
                {
                    APP_WARN("{}: No image card definition for card index {}", json_cards_path.string(), card_index);
                    continue;
                }

                std::string image = card["image"].GetString();

                parameters.push_back(DownloadManager::DownloadParameter {
                                        fmt::format("{0}/high.jpg", image),
                                        fmt::format("data/{0}/{1}/{2}_high_{3}.jpg", lang_id, set_id, local_id, sanitizeForPath(name))}
                                        );
            }
        }
    }

    for (const auto results = download_manager.download(parameters); const auto& result : results)
    {
        if (result.success)
        {
            APP_TRACE("{} -> Success ({})",
                result.effective_url,
                result.has_changed ? "Has changed" : "no changes");
        }
        else
        {
            APP_TRACE("{} -> ERROR: {}",
                result.effective_url,
                result.error);
        }
    }
}

int main()
{
    Logs::Initialize();

    APP_INFO("Application started.");

    DatabaseManager dbManager;

    if (!dbManager.open("metadata.db"))
    {
        std::cerr << "Failed to open database." << std::endl;
        return EXIT_FAILURE;
    }

    const auto downloadManager = DownloadManager(dbManager);

    const std::map<std::string, std::string> languages = {
        {"en", "English"},
        {"fr", "Français"},
        {"es", "Español"},
        {"es-mx", "Español (México)"},
        {"it", "Italiano"},
        {"pt", "Português"},
        {"pt-br", "Português (Brasil)"},
        {"pt-pt", "Português (Portugal)"},
        {"de", "Deutsch"},
        {"nl", "Nederlands"},
        {"pl", "Polski"},
        {"ru", "Русский"},
        {"ja", "日本語"},
        {"ko", "한국어"},
        {"zh-tw", "中文（台灣）"},
        {"id", "Bahasa Indonesia"},
        {"th", "ภาษาไทย"},
        {"zh-cn", "中文"}
    };

    refreshAllSets(downloadManager, languages);

    refreshAllCards(downloadManager);

    downloadCards(downloadManager);

    dbManager.close();

    APP_INFO("Application stop.");

    return EXIT_SUCCESS;
}
