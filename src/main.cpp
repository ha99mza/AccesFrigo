#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDebug>
#include "canhistory.h"
#include "accesscontrol.h"
#include "badgereader.h"
#include <memory>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("Frigo");
    app.setApplicationName("IHM");
    QCommandLineParser parser;
    parser.setApplicationDescription("IHM réfrigérateur 1024 × 600 · C++ / Qt");
    parser.addHelpOption();
    parser.addOption({"simulate", "Utiliser une carte simulée"});
    parser.addOption({"fullscreen", "Afficher en plein écran"});
    parser.addOption({"interface", "Interface SocketCAN", "name", "can0"});
    parser.addOption({"settings-file", "Fichier JSON des reglages confirmes", "path"});
    parser.addOption({"database", "Historique SQLite (defaut : ~/frigo.sqlite)", "path"});
    parser.addOption({"cloud-config", "Configuration du reporting cloud (defaut : ~/cloud.json)", "path"});
    parser.addOption({"badge-port", "Port serie du lecteur de badges (ex. /dev/ttyUSB0, COM3)", "port"});
    parser.addOption({"badge-baud", "Debit du lecteur de badges", "baud", "9600"});
    parser.addOption({"badge-test", "Simuler la lecture d'un badge au demarrage (simulation)", "code"});
    parser.addOption({"can-timeout", "Relancer CAN apres N secondes sans trame (0 desactive)", "seconds", "10"});
    parser.addOption({"alarm-start", "Chemin de l’exécutable qui active l’alarme", "path"});
    parser.addOption({"alarm-stop", "Chemin de l’exécutable qui arrête l’alarme", "path"});
    parser.addOption({"screenshot", "Enregistrer une capture puis quitter (simulation)", "path"});
    parser.addOption({"preview-page", "Écran à capturer en simulation", "name", "temp"});
    parser.process(app);
    bool validTimeout=false;const int timeout=parser.value("can-timeout").toInt(&validTimeout);
    if(!validTimeout||timeout<0||timeout>3600){qCritical()<<"--can-timeout doit etre compris entre 0 et 3600";return 2;}
    Controller controller(parser.isSet("simulate"), parser.value("interface"), nullptr, parser.value("settings-file"));
    std::unique_ptr<CanHistory> history;
    QString path=parser.value("database");if(path.isEmpty())path=QDir::homePath()+"/frigo.sqlite";
    if(path.startsWith("~/"))path=QDir::homePath()+path.mid(1);
    const bool persistent=!controller.simulation||parser.isSet("database");
    if(persistent){
        history=std::make_unique<CanHistory>(path);
        QObject::connect(&controller,&Controller::frameReceived,history.get(),&CanHistory::append);
        QObject::connect(&controller,&Controller::changed,history.get(),[&]{
            QJsonObject context{{"maintenanceMode",controller.maintenanceActive}};
            if(controller.synced){
                context.insert("settingsTempMax",controller.config[1]/10.0);
                context.insert("settingsTempMin",controller.config[0]/10.0);
                context.insert("settingsEvapMin",controller.config[2]/10.0);
            }
            history->setCloudContext(context);
        });
        QObject::connect(history.get(),&CanHistory::error,&controller,[&](QString error){
            qCritical().noquote()<<"SQLite:"<<error;controller.status="Erreur historique SQLite : "+error;emit controller.changed();
        });
        if(!controller.simulation){
            QString file=parser.value("cloud-config");if(file.isEmpty())file=QDir::homePath()+"/cloud.json";
            if(file.startsWith("~/"))file=QDir::homePath()+file.mid(1);
            history->configureCloud(file);
        }
    }
#ifdef Q_OS_LINUX
    controller.enableRecovery(timeout*1000);
#endif
    controller.alarmStart = parser.value("alarm-start");
    controller.alarmStop = parser.value("alarm-stop");
    // Badges live in the same SQLite file as the history; the plain simulation keeps a demo badge in memory.
    BadgeStore badges;
    if(!badges.open(persistent?path:QString(":memory:")))qCritical().noquote()<<"SQLite badges:"<<badges.error;
    else if(!persistent)badges.add("1234567890","Badge de démonstration");
    AccessControl access(&controller,&badges);
    BadgeReader reader;
    Window window(&controller,history.get());
    QObject::connect(&reader,&BadgeReader::badgeRead,&access,&AccessControl::check);
    QObject::connect(&access,&AccessControl::granted,&window,[&](QString,QString owner){
        window.notify(true,"Accès autorisé",owner+"\nOuverture de la porte");
    });
    QObject::connect(&access,&AccessControl::denied,&window,[&](QString,QString reason){
        window.notify(false,"Accès refusé",reason);
    });
    QObject::connect(&reader,&BadgeReader::error,&controller,[&](QString error){
        qCritical().noquote()<<"Badge:"<<error;controller.status=error;emit controller.changed();
    });
    if (parser.isSet("fullscreen"))
        window.showFullScreen();
    else
        window.show();
    if(parser.isSet("badge-port")){
        bool validBaud=false;const int baud=parser.value("badge-baud").toInt(&validBaud);
        if(!validBaud||baud<=0){qCritical()<<"--badge-baud doit etre un entier positif";return 2;}
        // Queued so the status set by a failed open is not overwritten by the controller start.
        QTimer::singleShot(100,&reader,[&,baud]{reader.open(parser.value("badge-port"),baud);});
    }
    if(parser.isSet("badge-test")&&controller.simulation)
        QTimer::singleShot(1000,&access,[&]{access.check(parser.value("badge-test"));});
    QTimer::singleShot(0, &controller, &Controller::start);
    if (parser.isSet("screenshot") && controller.simulation)
        QTimer::singleShot(2200, &window, [&] {
            window.preview(parser.value("preview-page"));
            window.grab().save(parser.value("screenshot"));
            app.quit();
        });
    return app.exec();
}
