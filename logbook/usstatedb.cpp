#include "usstatedb.h"
#include <QSqlQuery>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

USStateDB::USStateDB()
{
    auto const file_name = "USState.db";
    QString path;
    QDir dataPath {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
    path = dataPath.absoluteFilePath (file_name)

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);

    if (!db.open())
       {
          qDebug() << "Error: connection with database failed";
       }
       else
       {
          qDebug() << "Database: connection ok";
       }
}

QString USStateDB::getState(QString callsign)  {
    QString state =  "N/A";
    QSqlQuery query(db);
    query.prepare("SELECT state FROM USState WHERE call = (:call)");
    query.bindValue(":call", callsign);
    if (query.exec())
    {
       if (query.next())
       {
          state = query.value(0).toString();
       }
    }

    return state;
}


