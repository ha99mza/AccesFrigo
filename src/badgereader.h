#pragma once
#include <QObject>
#include <QSerialPort>
#include <QElapsedTimer>

// Lecteur de badges sur UART : trame binaire STX, longueur, type, UID, XOR, ETX.
class BadgeReader : public QObject {
 Q_OBJECT
public:
 explicit BadgeReader(QObject *parent=nullptr);
 bool open(const QString &portName,int baudRate=9600);
 void feed(const QByteArray &bytes);
signals:
 void badgeRead(QString code);
 void error(QString message);
private:
 friend class AccessTests;
 QSerialPort port;
 QByteArray buffer;
 QString lastCode;
 QElapsedTimer lastRead, gap;
 int repeatMs=3000;
 void complete(const QByteArray &frame);
};
