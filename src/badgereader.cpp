#include "badgereader.h"

BadgeReader::BadgeReader(QObject *parent):QObject(parent){
 connect(&port,&QSerialPort::readyRead,this,[this]{feed(port.readAll());});
 connect(&port,&QSerialPort::errorOccurred,this,[this](QSerialPort::SerialPortError e){
  // Open failures are reported by open(); this covers a reader unplugged while running.
  if(e!=QSerialPort::ResourceError)return;
  const auto reason=port.errorString();port.close();buffer.clear();
  emit error("Lecteur de badges deconnecte : "+reason);
 });
}
bool BadgeReader::open(const QString &portName,int baudRate){
 if(port.isOpen())port.close();
 port.setPortName(portName);port.setBaudRate(baudRate);
 port.setDataBits(QSerialPort::Data8);port.setParity(QSerialPort::NoParity);
 port.setStopBits(QSerialPort::OneStop);port.setFlowControl(QSerialPort::NoFlowControl);
 if(!port.open(QIODevice::ReadOnly)){emit error("Lecteur de badges inaccessible ("+portName+") : "+port.errorString());return false;}
 buffer.clear();return true;
}
void BadgeReader::feed(const QByteArray &bytes){
 // A pause means the previous frame was truncated: never glue it to the next badge.
 if(!buffer.isEmpty()&&gap.isValid()&&gap.elapsed()>200)buffer.clear();
 gap.start();buffer.append(bytes);
 // Frame: STX, total length, type, UID, XOR of length..UID, ETX (observed: 02 09 01 b2 f8 a9 49 a2 03).
 // UID bytes can equal STX or ETX, so frames are delimited by length and checksum only.
 while(true){
  const int start=int(buffer.indexOf(char(0x02)));
  if(start<0){buffer.clear();return;}
  buffer.remove(0,start);
  if(buffer.size()<2)return;
  const int length=quint8(buffer[1]);
  if(length<5||length>32){buffer.remove(0,1);continue;}
  if(buffer.size()<length)return;
  char sum=0;for(int i=1;i<length-2;++i)sum^=buffer[i];
  if(buffer[length-1]!=char(0x03)||buffer[length-2]!=sum){buffer.remove(0,1);continue;}
  complete(buffer.left(length));buffer.remove(0,length);
 }
}
void BadgeReader::complete(const QByteArray &frame){
 // The badge code is the whole frame in hexadecimal, e.g. 020901B2F8A949A203.
 const auto code=QString::fromLatin1(frame.toHex().toUpper());
 // A reader repeats the frame while the badge stays on the antenna.
 const bool repeated=code==lastCode&&lastRead.isValid()&&lastRead.elapsed()<repeatMs;
 lastCode=code;lastRead.start();
 if(!repeated)emit badgeRead(code);
}
