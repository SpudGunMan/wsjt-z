#ifndef USSTATEDB_H
#define USSTATEDB_H
#include <QString>
#include <QSqlDatabase>


class USStateDB
{
public:
    USStateDB();
    QString getState(QString callsign);

private:
    QSqlDatabase db;
};

#endif // USSTATEDB_H
