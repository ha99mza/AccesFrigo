#include "badgestore.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QVariant>

BadgeStore::~BadgeStore(){close();}
QString BadgeStore::normalize(const QString &code){return code.trimmed().toUpper();}
bool BadgeStore::open(const QString &path){
 close();error.clear();
 if(path!=":memory:"&&!QDir().mkpath(QFileInfo(path).absolutePath())){error="Dossier SQLite inaccessible : "+path;return false;}
 db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString());
 db.setDatabaseName(path);db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=2000");
 if(!db.open()){error=db.lastError().text();close();return false;}
 QSqlQuery q(db);
 if(!q.exec("CREATE TABLE IF NOT EXISTS badges (badge_code TEXT PRIMARY KEY COLLATE NOCASE, proprietaire TEXT NOT NULL)")){
  error=q.lastError().text();q.finish();close();return false;
 }
 return true;
}
void BadgeStore::close(){
 if(!db.isValid())return;
 auto name=db.connectionName();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(name);
}
bool BadgeStore::add(const QString &code,const QString &owner){
 error.clear();
 if(!db.isOpen()){error="Base des badges fermee";return false;}
 QSqlQuery q(db);q.prepare("INSERT OR REPLACE INTO badges(badge_code,proprietaire) VALUES(?,?)");
 q.addBindValue(normalize(code));q.addBindValue(owner);
 if(!q.exec()){error=q.lastError().text();return false;}
 return true;
}
std::optional<QString> BadgeStore::owner(const QString &code){
 error.clear();
 if(!db.isOpen()){error="Base des badges fermee";return std::nullopt;}
 QSqlQuery q(db);q.prepare("SELECT proprietaire FROM badges WHERE badge_code=?");
 q.addBindValue(normalize(code));
 if(!q.exec()){error=q.lastError().text();return std::nullopt;}
 if(!q.next())return std::nullopt;
 return q.value(0).toString();
}
