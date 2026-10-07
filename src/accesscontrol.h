#pragma once
#include "badgestore.h"
#include "controller.h"
#include <QObject>

// Decides what a badge read does: known badge -> door command on CAN, otherwise refusal.
class AccessControl : public QObject {
 Q_OBJECT
public:
 AccessControl(Controller *controller,BadgeStore *store,QObject *parent=nullptr);
public slots:
 void check(const QString &code);
signals:
 void granted(QString code,QString owner);
 void denied(QString code,QString reason);
private:
 Controller *controller;
 BadgeStore *store;
};
