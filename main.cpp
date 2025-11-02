#include <iostream>
#include <string>
#include <vector>
#include <execution>
#include <curl/curl.h>
#include "DatabaseManager.h"
#include "DownloadManager.h"
#include "rapidjson/document.h"

#include <future>

template<typename Iterator, typename Func>
void parallelForWithThreadCount(Iterator begin, Iterator end, Func func, size_t numThreads) {
    size_t totalSize = std::distance(begin, end);
    size_t chunkSize = (totalSize + numThreads - 1) / numThreads;

    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < numThreads; ++i) {
        auto chunkBegin = begin;
        std::advance(chunkBegin, std::min(i * chunkSize, totalSize));

        auto chunkEnd = begin;
        std::advance(chunkEnd, std::min((i + 1) * chunkSize, totalSize));

        if (chunkBegin == chunkEnd) break;

        futures.push_back(std::async(std::launch::async, [chunkBegin, chunkEnd, func]() {
            std::for_each(chunkBegin, chunkEnd, func);
        }));
    }

    // Attendre que tous les threads finissent
    for (auto& future : futures) {
        future.wait();
    }
}

auto setupDatabase(const DatabaseManager& database_manager) -> bool
{
    if (!database_manager.beginTransaction())
    {
        std::cerr << "Failed to begin transaction." << std::endl;
        return false;
    }

    const std::vector<DatabaseManager::Language> languages = {
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

    for (auto & language : languages)
    {
        if (!database_manager.upsertLanguage(language))
        {
            std::cerr << "Failed to upsert language: " << language.id << std::endl;
        }
    }

    if (!database_manager.commit())
    {
        std::cerr << "Failed to commit transaction." << std::endl;
        return false;
    }

    std::cout << "Database setup successfully." << std::endl;

    return true;
}

auto refreshAllSets(const DatabaseManager& database_manager) -> void
{
    std::cout << "Refreshing all sets..." << std::endl;

    for (const auto & languages = database_manager.getAllLanguages(); auto & language : languages)
    {
        std::string url = std::format("https://api.tcgdex.net/v2/{0}/sets", language.id);
        std::string json_data;
        std::string etag;
        std::string last_update;
        bool has_change;

        if (auto allSetContent = database_manager.getAllSetsContent(language.id); allSetContent.has_value())
        {
            etag = allSetContent->etag;
            last_update = allSetContent->last_update;
        }

        bool is_ok = DownloadManager::downloadJson(url,
                                      json_data, etag, last_update,
                                      has_change);

        if (!is_ok)
        {
            std::cerr << "Fail to download set for language " << language.id << "." << std::endl;
            continue;
        }

        if (has_change)
        {
            if (!database_manager.upsertAllSetsContent({language.id, etag, last_update, json_data}))
            {
                std::cerr << "Fail to download set for language " << language.id << "." << std::endl;
                continue;
            }

            std::cout << "Sets for language " << language.id << " updated." << std::endl;
        }
        else
        {
            std::cout << "No sets change for language " << language.id << "." << std::endl;
        }
    }
}

auto refreshAllCards(const DatabaseManager& database_manager) -> void
{
    std::cout << "Refreshing all cards..." << std::endl;

    for (auto & all_sets_content : database_manager.getAllSetsContents())
    {
        rapidjson::Document doc;

        doc.Parse(all_sets_content.content.c_str());

        if (doc.HasParseError()) {
            // TODO : Remove the set in database... and all cards belonging to this set.
            std::cerr << "  JSON parsing error for language set " << all_sets_content.language_id <<  " at "
                      << doc.GetErrorOffset()  << ": " << doc.GetParseError() << std::endl;
            continue;
        }

        if (!doc.IsArray())
        {
            std::cerr << "  JSON parsing error for language set " << all_sets_content.language_id <<  ": root is not an array." << std::endl;
            continue;
        }

        std::cout << doc.Size() << " sets for language " << all_sets_content.language_id << std::endl;

        for (const auto& set: doc.GetArray())
        {
            if (!set.IsObject() || !set.HasMember("id") || !set["id"].IsString())
            {
                std::cerr << "  Invalid set format for language " << all_sets_content.language_id << std::endl;
                continue;
            }

            std::string set_id = set["id"].GetString();

            std::cout << "  Find cards for set " << set_id << "..." << std::endl;

            std::string url = std::format("https://api.tcgdex.net/v2/{0}/sets/{1}", all_sets_content.language_id, set_id);
            std::string json_data;
            std::string etag;
            std::string last_update;
            bool has_change;

            if (auto allCardsContent = database_manager.getAllCardsContent(all_sets_content.language_id, set_id); allCardsContent.has_value())
            {
                etag = allCardsContent->etag;
                last_update = allCardsContent->last_update;
            }

            bool is_ok = DownloadManager::downloadJson(url,
                                          json_data, etag, last_update,
                                          has_change);

            if (!is_ok)
            {
                std::cerr << "  Fail to download cards for set " << set_id << " in language " << all_sets_content.language_id << "." << std::endl;
                continue;
            }

            if (has_change)
            {
                if (!database_manager.upsertAllCardsContent({all_sets_content.language_id, set_id, etag, last_update, json_data}))
                {

                    std::cerr << "  Fail to download set " << set_id << " for language " << all_sets_content.language_id << "." << std::endl;
                    continue;
                }

                std::cout << "  Sets " << set_id << " for language " << all_sets_content.language_id << " updated." << std::endl;
            }
            else
            {
                std::cout << "  No change for set " << set_id << " in language " << all_sets_content.language_id << "." << std::endl;
            }
        }
    }
}

auto downloadCards(const DatabaseManager& database_manager) -> void
{
    std::cout << "Download all cards..." << std::endl;

    for (auto & all_cards_content : database_manager.getAllCardsContents())
    {
        rapidjson::Document doc;

        doc.Parse(all_cards_content.content.c_str());

        if (doc.HasParseError()) {
            // TODO : Remove the all_card
            std::cerr << "  JSON parsing error for language set " << all_cards_content.language_id <<  " at "
                      << doc.GetErrorOffset()  << ": " << doc.GetParseError() << std::endl;
            continue;
        }

        if (!doc.IsObject())
        {
            std::cerr << "  JSON parsing error for language set " << all_cards_content.language_id <<  ": root is not an object." << std::endl;
            continue;
        }

        if (!doc.HasMember("cards") || !doc["cards"].IsArray())
        {
            std::cerr << "  JSON parsing error for language set " << all_cards_content.language_id <<  ": no 'cards' array found." << std::endl;
            continue;
        }

        auto cards = doc["cards"].GetArray();

        std::cout << cards.Size() << " cards for set " << all_cards_content.set_id << " in language " << all_cards_content.language_id << std::endl;

        for (const auto& card: cards)
        {
            if (!card.IsObject() || !card.HasMember("localId") || !card["localId"].IsString())
            {
                std::cerr << "  No localId card definition for set " << all_cards_content.set_id << " in language " << all_cards_content.language_id << std::endl;
                continue;
            }

            std::string local_id = card["localId"].GetString();

            if (!card.IsObject() || !card.HasMember("name") || !card["name"].IsString())
            {
                std::cerr << "  No name card definition for set " << all_cards_content.set_id << " in language " << all_cards_content.language_id << std::endl;
                continue;
            }

            std::string name = card["name"].GetString();

            if (!card.IsObject() || !card.HasMember("image") || !card["image"].IsString())
            {
                std::cerr << "  No image card definition for card " << local_id << " in set " << all_cards_content.set_id << " in language " << all_cards_content.language_id << std::endl;
                continue;
            }

            std::string image = card["image"].GetString();

            std::string url = std::format("{0}/high.jpg", image);
            std::string destination = std::format("PokemonTrainingData/{0}_{1}_{2}_{3}/image.jpg", all_cards_content.language_id, all_cards_content.set_id, local_id, name);
            std::string etag;
            std::string last_update;
            bool has_change;

            if (auto cardContent = database_manager.getCardContent(url); cardContent.has_value())
            {
                etag = cardContent->etag;
                last_update = cardContent->last_update;
            }

            bool is_ok = DownloadManager::downloadFile(url,
                                          destination, etag, last_update,
                                          has_change);

            if (!is_ok)
            {
                std::cerr << "  Download " << url << " ERR download." << std::endl;
                continue;
            }

            if (has_change)
            {
                if (!database_manager.upsertCardContent({url, etag, last_update}))
                {
                    std::cerr << "  Download " << url << " ERR upsert." << std::endl;
                    continue;
                }

                std::cout << "  Download " << url << " OK updated." << std::endl;
            }
            else
            {
                std::cout << "  Download " << url << " OK no change." << std::endl;
            }
        }
    }
}

int main()
{
    DownloadManager::Initialize();
    DatabaseManager dbManager;

    if (!dbManager.open("pokemon_cards.db"))
    {
        std::cerr << "Failed to open database." << std::endl;
        return EXIT_FAILURE;
    }

    if (!setupDatabase(dbManager))
    {
        return EXIT_FAILURE;
    }

    refreshAllSets(dbManager);

    refreshAllCards(dbManager);

    downloadCards(dbManager);

    std::cout << "Fin du traitement." << std::endl;

    dbManager.close();

    return EXIT_SUCCESS;
}
