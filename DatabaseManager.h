//
// Created by Zéro Cool on 01/11/2025.
//

#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <sqlite3.h>
#include <string>
#include <optional>

class DatabaseManager {
    sqlite3* m_db{nullptr};
    sqlite3_stmt* m_selectUriMetadataStmt{nullptr};
    sqlite3_stmt* m_upsertUriMetadataStmt{nullptr};

public:
    DatabaseManager();
    ~DatabaseManager();

    struct UriMetadata {
        std::string uri;
        std::string etag;
        std::string last_update;
    };

    auto open(const std::string& path) -> bool;
    auto close() -> void;

    [[nodiscard]] auto beginTransaction() const -> bool;
    [[nodiscard]] auto commit() const -> bool;

    [[nodiscard]] auto getUriMetadata(const std::string& uri) const -> std::optional<UriMetadata>;
    [[nodiscard]] auto upsertUriMetadata(const UriMetadata& uri_metadata) const -> bool;

private:
    auto prepareStatements() -> void;
    [[nodiscard]] auto createModel() const -> bool;
};

#endif //DATABASE_MANAGER_H
