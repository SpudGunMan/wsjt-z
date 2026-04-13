#include "qrzlookup.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QHash>
#include <QXmlStreamReader>

QRZLookup::QRZLookup(QString un, QString pw)
{
    _username = un;
    _password = pw;
    _networkManager = new QNetworkAccessManager();
    initialize();
}

void QRZLookup::initialize() {
        QUrlQuery query;
        query.addQueryItem("username", _username);
        query.addQueryItem("password", _password);
        QUrl url(_url);
        url.setQuery(query);
        QNetworkRequest networkRequest(url);
        connect(_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(setSessionKey(QNetworkReply*)));
        _networkManager->get(networkRequest);

}

void QRZLookup::setSessionKey(QNetworkReply *r) {
    if(r->error() == QNetworkReply::NoError) {
        QByteArray data = r->read(2048);
        QXmlStreamReader reader(data);
        while(!reader.atEnd() && !reader.hasError()) {
            if(reader.readNext() == QXmlStreamReader::StartElement && reader.name() == "Key") {
               _sessionKey = reader.readElementText();
            }
        }
    }
    r->deleteLater();
}

void QRZLookup::lookup(QString dxCall) {
    QHash<QString, QString> hash;
    QUrlQuery query;
    query.addQueryItem("s", _sessionKey);
    query.addQueryItem("callsign", dxCall);
    QUrl url(_url);
    url.setQuery(query);
    QNetworkRequest networkRequest(url);
    connect(_networkManager, SIGNAL(finished(QNetworkReply*)), MainWindow, SLOT(setSessionKey(QNetworkReply*)));
    _networkManager->get(networkRequest);
}




