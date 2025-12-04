// EAPUploadQueueManager.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QJsonDocument>
#include "EAPInterfaceManager.h"

class EAPCORE_EXPORT EAPUploadQueueManager : public QObject {
    Q_OBJECT
public:
    explicit EAPUploadQueueManager(EAPInterfaceManager* uploader, QObject* parent = nullptr);
    void submit(const QString& interfaceKey, const QVariantMap& params, bool fromQueue = false);
    void start();
    void stop();

private slots:
    void trySendNext();
    void onRequestFailed(const QString& key, const QString& error);
    void onMappedResultReady(const QString& key, const QVariantMap& result);

private:
    void initDb();
    void enqueue(const QString& interfaceKey, const QJsonObject& payload);
    bool dequeue(QString& id, QString& interfaceKey, QJsonObject& payload);
    void remove(const QString& id);

    QTimer timer; 
    QSqlDatabase db;
    EAPInterfaceManager* uploader;
    QString currentId; // 当前发送中的记录ID
};
