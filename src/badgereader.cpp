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
 for(const char ch:bytes){
  if(ch==0x02){buffer.clear();continue;} // STX starts a new frame.
  if(ch=='\r'||ch=='\n'||ch==0x03){complete();continue;}
  if(ch>0x20&&ch<0x7f&&buffer.size()<64)buffer.append(ch);
 }
}
void BadgeReader::complete(){
 const auto code=QString::fromLatin1(buffer);buffer.clear();
 if(code.isEmpty())return;
 // A reader repeats the frame while the badge stays on the antenna.
 const bool repeated=code==lastCode&&lastRead.isValid()&&lastRead.elapsed()<repeatMs;
 lastCode=code;lastRead.start();
 if(!repeated)emit badgeRead(code);
}
