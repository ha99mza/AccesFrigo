#include "accesscontrol.h"

AccessControl::AccessControl(Controller *c,BadgeStore *s,QObject *parent):QObject(parent),controller(c),store(s){}
void AccessControl::check(const QString &raw){
 const auto code=BadgeStore::normalize(raw);
 if(code.isEmpty())return;
 const auto owner=store->owner(code);
 if(!owner){
  emit denied(code,store->error.isEmpty()?"Badge inconnu : "+code:"Base des badges indisponible : "+store->error);
  return;
 }
 // A known badge is only announced as authorized if the door command could be queued.
 if(!controller->openDoor()){emit denied(code,"Badge de "+*owner+" reconnu, mais CAN déconnecté : porte non commandée");return;}
 emit granted(code,*owner);
}
