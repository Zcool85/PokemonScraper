//
// Created by Zéro Cool on 01/11/2025.
//

#include <iostream>

#include "DatabaseManager.h"
#include "Logs.h"

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    close();
}

auto DatabaseManager::open(const std::string& path) -> bool
{
    if (const int rc = sqlite3_open(path.c_str(), &m_db); rc != SQLITE_OK)
    {
        DB_ERROR("Open database error: {}", sqlite3_errmsg(m_db));

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
    if (m_selectUriMetadataStmt) sqlite3_finalize(m_selectUriMetadataStmt);
    if (m_upsertUriMetadataStmt) sqlite3_finalize(m_upsertUriMetadataStmt);

    if (m_db) sqlite3_close(m_db);

    m_db = nullptr;

    m_selectUriMetadataStmt = nullptr;
    m_upsertUriMetadataStmt = nullptr;
}

auto DatabaseManager::beginTransaction() const -> bool
{
    char* errMsg = nullptr;
    if (const int rc = sqlite3_exec(m_db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        DB_ERROR("BEGIN Error: {}", errMsg);

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
        DB_ERROR("COMMIT Error: {}", errMsg);

        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

auto DatabaseManager::getUriMetadata(const std::string& uri) const -> std::optional<UriMetadata>
{
    sqlite3_reset(m_selectUriMetadataStmt);
    sqlite3_bind_text(m_selectUriMetadataStmt, 1, uri.c_str(), -1, SQLITE_TRANSIENT);

    if (const int rc = sqlite3_step(m_selectUriMetadataStmt); rc == SQLITE_ROW) {
        const unsigned char* c_uri = sqlite3_column_text(m_selectUriMetadataStmt, 0);
        const unsigned char* c_etag = sqlite3_column_text(m_selectUriMetadataStmt, 1);
        const unsigned char* c_last_update = sqlite3_column_text(m_selectUriMetadataStmt, 2);

        return UriMetadata {
            c_uri ? reinterpret_cast<const char*>(c_uri) : "",
            c_etag ? reinterpret_cast<const char*>(c_etag) : "",
            c_last_update ? reinterpret_cast<const char*>(c_last_update) : ""
        };
    }

    return std::nullopt;
}

auto DatabaseManager::upsertUriMetadata(const UriMetadata& uri_metadata) const -> bool
{
    sqlite3_reset(m_upsertUriMetadataStmt);
    sqlite3_bind_text(m_upsertUriMetadataStmt, 1, uri_metadata.uri.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertUriMetadataStmt, 2, uri_metadata.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_upsertUriMetadataStmt, 3, uri_metadata.last_update.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(m_upsertUriMetadataStmt);
    return rc == SQLITE_DONE;
}

auto DatabaseManager::prepareStatements() -> void
{
    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(SELECT uri, etag, last_updated FROM uri_metadata WHERE uri = ?)",
        -1, &m_selectUriMetadataStmt, nullptr); rc != SQLITE_OK)
    {
        DB_ERROR("Prepare error for m_selectUriMetadataStmt: {}", sqlite3_errmsg(m_db));
    }

    if (const int rc = sqlite3_prepare_v2(m_db,
        R"(
            INSERT INTO uri_metadata (uri, etag, last_updated)
            VALUES (?, ?, ?)
            ON CONFLICT(uri) DO UPDATE SET
                etag=excluded.etag,
                last_updated=excluded.last_updated
        )",
        -1, &m_upsertUriMetadataStmt, nullptr); rc != SQLITE_OK)
    {
        DB_ERROR("Prepare error for m_upsertUriMetadataStmt: {}", sqlite3_errmsg(m_db));
    }
}

auto DatabaseManager::createModel() const -> bool
{
    char* errMsg = nullptr;

    const auto createUriMetadataTableSQL = R"(
            CREATE TABLE IF NOT EXISTS uri_metadata (
                uri TEXT PRIMARY KEY,
                etag TEXT NOT NULL,
                last_updated TEXT NOT NULL
            )
        )";

    if (const int rc = sqlite3_exec(m_db, createUriMetadataTableSQL, nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        DB_ERROR("CREATE TABLE languages error: {}", errMsg);

        sqlite3_free(errMsg);
        return false;
    }

    return true;
}
