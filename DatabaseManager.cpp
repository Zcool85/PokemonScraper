//
// Created by Zéro Cool on 01/11/2025.
//

#include "DatabaseManager.h"

#include <iostream>

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    close();
}

auto DatabaseManager::open(const std::string& path) -> bool
{
    if (const int rc = sqlite3_open(path.c_str(), &m_db); rc != SQLITE_OK)
    {
        std::cerr << "Open database error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    if (!createModel())
    {
        return false;
    }

    prepareStatements();

    return true;
}

auto DatabaseManager::close() -> void
{
    if (m_selectLanguageStmt) sqlite3_finalize(m_selectLanguageStmt);
    if (m_upsertLanguageStmt) sqlite3_finalize(m_upsertLanguageStmt);
    if (m_selectLanguagesStmt) sqlite3_finalize(m_selectLanguagesStmt);
    if (m_selectAllSetsByLanguageIdStmt) sqlite3_finalize(m_selectAllSetsByLanguageIdStmt);
    if (m_selectAllSetsStmt) sqlite3_finalize(m_selectAllSetsStmt);
    if (m_upsertAllSetsContentStmt) sqlite3_finalize(m_upsertAllSetsContentStmt);
    if (m_selectAllCardsByLanguageIdAndSetIdStmt) sqlite3_finalize(m_selectAllCardsByLanguageIdAndSetIdStmt);
    if (m_upsertAllCardsContentStmt) sqlite3_finalize(m_upsertAllCardsContentStmt);
    if (m_selectAllCardsStmt) sqlite3_finalize(m_selectAllCardsStmt);
    if (m_selectCardByUrlStmt) sqlite3_finalize(m_selectCardByUrlStmt);
    if (m_upsertCardStmt) sqlite3_finalize(m_upsertCardStmt);
    if (m_db) sqlite3_close(m_db);

    m_db = nullptr;
    m_selectLanguageStmt = nullptr;
    m_upsertLanguageStmt = nullptr;
    m_selectLanguagesStmt = nullptr;
    m_selectAllSetsByLanguageIdStmt = nullptr;
    m_selectAllSetsStmt = nullptr;
    m_upsertAllSetsContentStmt = nullptr;
    m_selectAllCardsByLanguageIdAndSetIdStmt = nullptr;
    m_upsertAllCardsContentStmt = nullptr;
    m_selectAllCardsStmt = nullptr;
    m_selectCardByUrlStmt = nullptr;
    m_upsertCardStmt = nullptr;
}

auto DatabaseManager::beginTransaction() const -> bool
{
    char* errMsg = nullptr;
    if (const int rc = sqlite3_exec(m_db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "BEGIN Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

auto DatabaseManager::commit() const -> bool
{
    char* errMsg = nullptr;
    if (const int rc = sqlite3_exec(m_db, "COMMIT", nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "COMMIT Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

auto DatabaseManager::upsertLanguage(const Language& language) const -> bool
{
    sqlite3_reset(m_upsertLanguageStmt);
    sqlite3_bind_text(m_upsertLanguageStmt, 1, language.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertLanguageStmt, 2, language.name.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(m_upsertLanguageStmt);
    return (rc == SQLITE_DONE);
}

auto DatabaseManager::getAllLanguages() const -> std::vector<Language>
{
    std::vector<Language> languages;

    sqlite3_reset(m_selectLanguagesStmt);

    while (sqlite3_step(m_selectLanguagesStmt) == SQLITE_ROW) {
        const unsigned char* c_id = sqlite3_column_text(m_selectLanguagesStmt, 0);
        const unsigned char* c_name = sqlite3_column_text(m_selectLanguagesStmt, 1);

        std::string id = c_id ? reinterpret_cast<const char*>(c_id) : "";
        std::string name = c_name ? reinterpret_cast<const char*>(c_name) : "";

        languages.emplace_back(id, name);
    }

    return languages;
}

auto DatabaseManager::getAllSetsContent(const std::string& language_id) const -> std::optional<AllSetsContent>
{
    sqlite3_reset(m_selectAllSetsByLanguageIdStmt);
    sqlite3_bind_text(m_selectAllSetsByLanguageIdStmt, 1, language_id.c_str(), -1, SQLITE_TRANSIENT);

    if (const int rc = sqlite3_step(m_selectAllSetsByLanguageIdStmt); rc == SQLITE_ROW) {
        const unsigned char* c_language_id = sqlite3_column_text(m_selectAllSetsByLanguageIdStmt, 0);
        const unsigned char* c_etag = sqlite3_column_text(m_selectAllSetsByLanguageIdStmt, 1);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectAllSetsByLanguageIdStmt, 2);
        const unsigned char* c_content = sqlite3_column_text(m_selectAllSetsByLanguageIdStmt, 3);

        return AllSetsContent {
            c_language_id ? reinterpret_cast<const char*>(c_language_id) : "",
            c_etag ? reinterpret_cast<const char*>(c_etag) : "",
            c_last_update ? reinterpret_cast<const char*>(c_last_update) : "",
            c_content ? reinterpret_cast<const char*>(c_content) : ""
        };
    }

    return std::nullopt;
}

auto DatabaseManager::getAllSetsContents() const -> std::vector<AllSetsContent>
{
    std::vector<AllSetsContent> all_sets_contents;

    sqlite3_reset(m_selectAllSetsStmt);

    while (sqlite3_step(m_selectAllSetsStmt) == SQLITE_ROW) {
        const unsigned char* c_language_id = sqlite3_column_text(m_selectAllSetsStmt, 0);
        const unsigned char* c_etag = sqlite3_column_text(m_selectAllSetsStmt, 1);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectAllSetsStmt, 2);
        const unsigned char* c_content = sqlite3_column_text(m_selectAllSetsStmt, 3);

        std::string language_id = c_language_id ? reinterpret_cast<const char*>(c_language_id) : "";
        std::string etag = c_etag ? reinterpret_cast<const char*>(c_etag) : "";
        std::string last_update = c_last_update ? reinterpret_cast<const char*>(c_last_update) : "";
        std::string content = c_content ? reinterpret_cast<const char*>(c_content) : "";

        all_sets_contents.emplace_back(language_id, etag, last_update, content);
    }

    return all_sets_contents;
}

auto DatabaseManager::upsertAllSetsContent(const AllSetsContent& all_sets_content) const -> bool
{
    sqlite3_reset(m_upsertAllSetsContentStmt);
    sqlite3_bind_text(m_upsertAllSetsContentStmt, 1, all_sets_content.language_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllSetsContentStmt, 2, all_sets_content.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllSetsContentStmt, 3, all_sets_content.last_update.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllSetsContentStmt, 4, all_sets_content.content.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(m_upsertAllSetsContentStmt);
    return (rc == SQLITE_DONE);
}

auto DatabaseManager::getAllCardsContent(const std::string& language_id,
    const std::string& set_id) const -> std::optional<AllCardsContent>
{
    sqlite3_reset(m_selectAllCardsByLanguageIdAndSetIdStmt);
    sqlite3_bind_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 1, language_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 2, set_id.c_str(), -1, SQLITE_TRANSIENT);

    if (const int rc = sqlite3_step(m_selectAllCardsByLanguageIdAndSetIdStmt); rc == SQLITE_ROW) {
        const unsigned char* c_language_id = sqlite3_column_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 0);
        const unsigned char* c_set_id = sqlite3_column_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 1);
        const unsigned char* c_etag = sqlite3_column_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 2);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 3);
        const unsigned char* c_content = sqlite3_column_text(m_selectAllCardsByLanguageIdAndSetIdStmt, 4);

        return AllCardsContent {
            c_language_id ? reinterpret_cast<const char*>(c_language_id) : "",
            c_set_id ? reinterpret_cast<const char*>(c_set_id) : "",
            c_etag ? reinterpret_cast<const char*>(c_etag) : "",
            c_last_update ? reinterpret_cast<const char*>(c_last_update) : "",
            c_content ? reinterpret_cast<const char*>(c_content) : ""
        };
    }

    return std::nullopt;
}

auto DatabaseManager::getAllCardsContents() const -> std::vector<AllCardsContent>
{
    std::vector<AllCardsContent> all_cards_contents;

    sqlite3_reset(m_selectAllCardsStmt);

    while (sqlite3_step(m_selectAllCardsStmt) == SQLITE_ROW) {
        const unsigned char* c_language_id = sqlite3_column_text(m_selectAllCardsStmt, 0);
        const unsigned char* c_set_id = sqlite3_column_text(m_selectAllCardsStmt, 1);
        const unsigned char* c_etag = sqlite3_column_text(m_selectAllCardsStmt, 2);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectAllCardsStmt, 3);
        const unsigned char* c_content = sqlite3_column_text(m_selectAllCardsStmt, 4);

        std::string language_id = c_language_id ? reinterpret_cast<const char*>(c_language_id) : "";
        std::string set_id = c_set_id ? reinterpret_cast<const char*>(c_set_id) : "";
        std::string etag = c_etag ? reinterpret_cast<const char*>(c_etag) : "";
        std::string last_update = c_last_update ? reinterpret_cast<const char*>(c_last_update) : "";
        std::string content = c_content ? reinterpret_cast<const char*>(c_content) : "";

        all_cards_contents.emplace_back(language_id, set_id, etag, last_update, content);
    }

    return all_cards_contents;
}

auto DatabaseManager::upsertAllCardsContent(const AllCardsContent& all_cards_content) const -> bool
{
    sqlite3_reset(m_upsertAllCardsContentStmt);
    sqlite3_bind_text(m_upsertAllCardsContentStmt, 1, all_cards_content.language_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllCardsContentStmt, 2, all_cards_content.set_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllCardsContentStmt, 3, all_cards_content.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllCardsContentStmt, 4, all_cards_content.last_update.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertAllCardsContentStmt, 5, all_cards_content.content.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(m_upsertAllCardsContentStmt);
    return (rc == SQLITE_DONE);
}

auto DatabaseManager::getCardContent(const std::string& url) const -> std::optional<CardContent>
{
    sqlite3_reset(m_selectCardByUrlStmt);
    sqlite3_bind_text(m_selectCardByUrlStmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

    if (const int rc = sqlite3_step(m_selectCardByUrlStmt); rc == SQLITE_ROW) {
        const unsigned char* c_url = sqlite3_column_text(m_selectCardByUrlStmt, 0);
        const unsigned char* c_etag = sqlite3_column_text(m_selectCardByUrlStmt, 1);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectCardByUrlStmt, 2);

        return CardContent {
            c_url ? reinterpret_cast<const char*>(c_url) : "",
            c_etag ? reinterpret_cast<const char*>(c_etag) : "",
            c_last_update ? reinterpret_cast<const char*>(c_last_update) : ""
        };
    }

    return std::nullopt;
}

auto DatabaseManager::upsertCardContent(const CardContent& card_content) const -> bool
{
    sqlite3_reset(m_upsertCardStmt);
    sqlite3_bind_text(m_upsertCardStmt, 1, card_content.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertCardStmt, 2, card_content.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertCardStmt, 3, card_content.last_update.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(m_upsertCardStmt);
    return (rc == SQLITE_DONE);
}

auto DatabaseManager::prepareStatements() -> void
{
    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT name FROM languages WHERE id = ?)",
        -1, &m_selectLanguageStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectLanguageStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(INSERT INTO languages (id, name) VALUES (?, ?) ON CONFLICT(id) DO UPDATE SET name=excluded.name)",
        -1, &m_upsertLanguageStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_upsertLanguageStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT id, name FROM languages ORDER BY id)",
        -1, &m_selectLanguagesStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectLanguagesStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT language_id, etag, last_update, content FROM all_sets WHERE language_id = ?)",
        -1, &m_selectAllSetsByLanguageIdStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectAllSetsByLanguageIdStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT language_id, etag, last_update, content FROM all_sets ORDER BY language_id)",
        -1, &m_selectAllSetsStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectAllSetsStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(
            INSERT INTO all_sets (language_id, etag, last_update, content)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(language_id) DO UPDATE SET
                etag=excluded.etag,
                last_update=excluded.last_update,
                content=excluded.content
        )",
        -1, &m_upsertAllSetsContentStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_upsertAllSetsContentStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT language_id, set_id, etag, last_update, content FROM all_cards WHERE language_id = ? AND set_id = ?)",
        -1, &m_selectAllCardsByLanguageIdAndSetIdStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectAllCardsByLanguageIdAndSetIdStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(
            INSERT INTO all_cards (language_id, set_id, etag, last_update, content)
            VALUES (?, ?, ?, ?, ?)
            ON CONFLICT(language_id, set_id) DO UPDATE SET
                etag=excluded.etag,
                last_update=excluded.last_update,
                content=excluded.content
        )",
        -1, &m_upsertAllCardsContentStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_upsertAllCardsContentStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT language_id, set_id, etag, last_update, content FROM all_cards ORDER BY language_id, set_id)",
        -1, &m_selectAllCardsStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectAllCardsStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT uri, etag, last_update FROM cards WHERE uri = ?)",
        -1, &m_selectCardByUrlStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_selectCardByUrlStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(
            INSERT INTO cards (uri, etag, last_update)
            VALUES (?, ?, ?)
            ON CONFLICT(uri) DO UPDATE SET
                etag=excluded.etag,
                last_update=excluded.last_update
        )",
        -1, &m_upsertCardStmt, nullptr); rc != SQLITE_OK)
    {
        std::cerr << "Prepare error for m_upsertCardStmt: " << sqlite3_errmsg(m_db) << std::endl;
    }
}

auto DatabaseManager::createModel() const -> bool
{
    char* errMsg = nullptr;

    const auto createLanguagesTableSQL = R"(
            CREATE TABLE IF NOT EXISTS languages (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL
            )
        )";

    if (const int rc = sqlite3_exec(m_db, createLanguagesTableSQL, nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "CREATE TABLE languages error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }

    const auto createAllSetsTableSQL = R"(
            CREATE TABLE IF NOT EXISTS all_sets (
                language_id TEXT PRIMARY KEY,
                etag TEXT NOT NULL,
                last_update TEXT NOT NULL,
                content JSON NOT NULL,
                FOREIGN KEY(language_id) REFERENCES languages(id)
            )
        )";

    if (const int rc = sqlite3_exec(m_db, createAllSetsTableSQL, nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "CREATE TABLE all_sets error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }

    const auto createAllCardsTableSQL = R"(
            CREATE TABLE IF NOT EXISTS all_cards (
                language_id TEXT NOT NULL,
                set_id TEXT NOT NULL,
                etag TEXT NOT NULL,
                last_update TEXT NOT NULL,
                content JSON NOT NULL,
                PRIMARY KEY (language_id, set_id)
            )
        )";

    if (const int rc = sqlite3_exec(m_db, createAllCardsTableSQL, nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "CREATE TABLE all_cards error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }

    const auto createCardsTableSQL = R"(
            CREATE TABLE IF NOT EXISTS cards (
                uri TEXT PRIMARY KEY,
                etag TEXT NOT NULL,
                last_update TEXT NOT NULL
            )
        )";

    if (const int rc = sqlite3_exec(m_db, createCardsTableSQL, nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        std::cerr << "CREATE TABLE cards error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}
