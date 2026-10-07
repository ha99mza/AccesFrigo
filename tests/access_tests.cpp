#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "accesscontrol.h"
#include "badgereader.h"
#include "canhistory.h"

class AccessTests : public QObject {
 Q_OBJECT
private slots:
 void readerSplitsFramesAndIgnoresRepeats(){
  BadgeReader r;QSignalSpy read(&r,&BadgeReader::badgeRead);
  const auto frame=QByteArray::fromHex("020901b2f8a949a203"); // Captured from the real reader.
  r.feed(frame.left(4));QCOMPARE(read.size(),0); // Frame split across two reads.
  r.feed(frame.mid(4));QCOMPARE(read.size(),1);QCOMPARE(read[0][0].toString(),QString("020901B2F8A949A203"));
  r.feed(frame);QCOMPARE(read.size(),1); // Badge still on the antenna.
  // UID containing STX and ETX bytes, preceded by noise that looks like a frame start.
  const auto tricky=QByteArray::fromHex("0209010203020308")+QByteArray::fromHex("03");
  r.feed(QByteArray::fromHex("ff0207")+tricky);QCOMPARE(read.size(),2);QCOMPARE(read[1][0].toString(),QString("020901020302030803"));
  r.repeatMs=0;
  r.feed(QByteArray::fromHex("020901b2f8a949a303"));QCOMPARE(read.size(),2); // Wrong checksum.
  r.feed(frame+frame);QCOMPARE(read.size(),4);
  r.feed(frame.left(5));QTest::qWait(250);r.feed(frame);QCOMPARE(read.size(),5); // Truncated frame is dropped after a pause.
 }
 void storePersistsNextToHistory(){
  QTemporaryDir dir;const auto path=dir.filePath("frigo.sqlite");
  HistoryWriter writer;writer.open(path);
  {
   BadgeStore s;QVERIFY2(s.open(path),qPrintable(s.error));
   QVERIFY(s.add(" 0a1b2c ","Hamza"));QVERIFY(s.add("0A1B2C","Hamza L."));
   QCOMPARE(s.owner("0a1b2c\r").value_or("?"),QString("Hamza L."));
   QVERIFY(!s.owner("FFFF"));QVERIFY(s.error.isEmpty());
  }
  QSignalSpy errors(&writer,&HistoryWriter::error);
  writer.append(1,QByteArray(1,char(0x52)),30000);QVERIFY(errors.isEmpty());writer.close(); // History still writes.
  BadgeStore reopened;QVERIFY(reopened.open(path));QCOMPARE(reopened.owner("0A1B2C").value_or("?"),QString("Hamza L."));
  BadgeStore closed;QVERIFY(!closed.owner("0A1B2C"));QVERIFY(!closed.error.isEmpty());
 }
 void knownBadgeOpensDoorOnCan(){
  Controller c(true,"test");BadgeStore s;QVERIFY(s.open(":memory:"));QVERIFY(s.add("020901b2f8a949a203","Hamza"));
  AccessControl a(&c,&s);BadgeReader r;connect(&r,&BadgeReader::badgeRead,&a,&AccessControl::check);
  QSignalSpy granted(&a,&AccessControl::granted),denied(&a,&AccessControl::denied),sent(&c,&Controller::transmitted);
  r.feed(QByteArray::fromHex("020901b2f8a949a203"));
  QCOMPARE(granted.size(),1);QCOMPARE(denied.size(),0);QCOMPARE(granted[0][1].toString(),QString("Hamza"));
  QTRY_COMPARE(sent.size(),1);
  QCOMPARE(sent[0][0].toUInt(),uint(0x107));QCOMPARE(sent[0][1].toByteArray(),QByteArray::fromHex("08"));QVERIFY(!sent[0][2].toBool());
 }
 void unknownBadgeSendsNothing(){
  Controller c(true,"test");BadgeStore s;QVERIFY(s.open(":memory:"));
  AccessControl a(&c,&s);
  QSignalSpy granted(&a,&AccessControl::granted),denied(&a,&AccessControl::denied),sent(&c,&Controller::transmitted);
  a.check("DEAD");QCOMPARE(denied.size(),1);QCOMPARE(granted.size(),0);QVERIFY(denied[0][1].toString().contains("DEAD"));
  QTest::qWait(700);QCOMPARE(sent.size(),0);
 }
 void doorFramePassesQueuedSettings(){
  Controller c(true,"test");c.start();QTRY_VERIFY(c.synced);
  auto values=c.config;values[0]+=5;values[1]+=5;
  QSignalSpy sent(&c,&Controller::transmitted);
  c.save(values);QVERIFY(c.openDoor());
  QTRY_COMPARE(sent.size(),1);QCOMPARE(sent[0][0].toUInt(),uint(0x107));
  QTRY_VERIFY_WITH_TIMEOUT(c.synced,8000);QCOMPARE(c.config[0],values[0]);
 }
 void disconnectedCanRefusesKnownBadge(){
  Controller c(false,"none");BadgeStore s;QVERIFY(s.open(":memory:"));QVERIFY(s.add("0A1B2C","Hamza"));
  AccessControl a(&c,&s);QSignalSpy granted(&a,&AccessControl::granted),denied(&a,&AccessControl::denied);
  a.check("0A1B2C");QCOMPARE(granted.size(),0);QCOMPARE(denied.size(),1);QVERIFY(denied[0][1].toString().contains("CAN"));
 }
};
QTEST_GUILESS_MAIN(AccessTests)
#include "access_tests.moc"
