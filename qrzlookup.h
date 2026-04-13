#ifndef QRZLOOKUP_H
#define QRZLOOKUP_H

#include <QNetworkAccessManager>
#include <QString>
#include <QHash>




class QRZLookup  : public QObject
{
    Q_OBJECT

public:
    QRZLookup(QString un, QString pw);
    void lookup(QString dxCall);

private:
    QString _username;
    QString _password;
    QString _url = "https://xmldata.qrz.com/xml/";
    QString _sessionKey;
    QNetworkAccessManager * _networkManager;
    void initialize();
    void setSessionKey(QNetworkReply *r);
    void parseResponse(QNetworkReply * r);

};

#endif // QRZLOOKUP_H
