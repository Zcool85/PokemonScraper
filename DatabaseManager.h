//
// Created by Zéro Cool on 01/11/2025.
//

#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <sqlite3.h>
#include <vector>
#include <string>
#include <optional>

class DatabaseManager {
    sqlite3* m_db{nullptr};
    sqlite3_stmt* m_selectLanguageStmt{nullptr};
    sqlite3_stmt* m_upsertLanguageStmt{nullptr};
    sqlite3_stmt* m_selectLanguagesStmt{nullptr};
    sqlite3_stmt* m_selectAllSetsByLanguageIdStmt{nullptr};
    sqlite3_stmt* m_selectAllSetsStmt{nullptr};
    sqlite3_stmt* m_upsertAllSetsContentStmt{nullptr};
    sqlite3_stmt* m_selectAllCardsByLanguageIdAndSetIdStmt{nullptr};
    sqlite3_stmt* m_upsertAllCardsContentStmt{nullptr};
    sqlite3_stmt* m_selectAllCardsStmt{nullptr};
    sqlite3_stmt* m_selectCardByUrlStmt{nullptr};
    sqlite3_stmt* m_upsertCardStmt{nullptr};

public:
    DatabaseManager();
    ~DatabaseManager();

    struct Language {
        std::string id;
        std::string name;
    };

    struct AllSetsContent {
        std::string language_id;
        std::string etag;
        std::string last_update;
        std::string content;
    };

    struct AllCardsContent {
        std::string language_id;
        std::string set_id;
        std::string etag;
        std::string last_update;
        std::string content;
    };

    struct CardContent {
        std::string url;
        std::string etag;
        std::string last_update;
    };

    auto open(const std::string& path) -> bool;
    auto close() -> void;
    [[nodiscard]] auto beginTransaction() const -> bool;
    [[nodiscard]] auto commit() const -> bool;

    [[nodiscard]] auto upsertLanguage(const Language& language) const -> bool;
    [[nodiscard]] auto getAllLanguages() const -> std::vector<Language>;

    [[nodiscard]] auto getAllSetsContent(const std::string& language_id) const -> std::optional<AllSetsContent>;
    [[nodiscard]] auto getAllSetsContents() const -> std::vector<AllSetsContent>;
    [[nodiscard]] auto upsertAllSetsContent(const AllSetsContent& all_sets_content) const -> bool;

    [[nodiscard]] auto getAllCardsContent(const std::string& language_id, const std::string& set_id) const -> std::optional<AllCardsContent>;
    [[nodiscard]] auto getAllCardsContents() const -> std::vector<AllCardsContent>;
    [[nodiscard]] auto upsertAllCardsContent(const AllCardsContent& all_cards_content) const -> bool;

    [[nodiscard]] auto getCardContent(const std::string& url) const -> std::optional<CardContent>;
    [[nodiscard]] auto upsertCardContent(const CardContent& card_content) const -> bool;

private:
    auto prepareStatements() -> void;
    [[nodiscard]] auto createModel() const -> bool;
};

#endif //DATABASE_MANAGER_H
