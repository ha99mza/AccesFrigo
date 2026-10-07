#pragma once
#include <QSqlDatabase>
#include <QString>
#include <optional>

// Table badges(badge_code, proprietaire). The connection belongs to the calling thread.
class BadgeStore {
public:
 ~BadgeStore();
 bool open(const QString &path);
 void close();
 bool isOpen() const {return db.isOpen();}
 bool add(const QString &code,const QString &owner);
 std::optional<QString> owner(const QString &code);
 static QString normalize(const QString &code);
 QString error;
private:
 QSqlDatabase db;
};
