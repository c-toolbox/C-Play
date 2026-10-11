/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tcpcontrolrestapi.h"
#include "tcpcontrolmanager.h"
#ifdef OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <cpp-httplib/httplib.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QThread>
#include <atomic>
#include <future>

namespace {
struct Reply {
    int status = 200;
    QJsonObject body;
};

struct PendingReply {
    Reply reply;
    std::promise<Reply> completed;
    std::atomic<bool> expired = false;
};

Reply failure(int status, const QString &message) {
    return {status, {{QStringLiteral("success"), false}, {QStringLiteral("message"), message}}};
}
}

void registerTcpControlRoutes(httplib::Server &server, TcpControlManager *manager, const TcpPostRegistrar &registerPost) {
    const QPointer<TcpControlManager> guarded(manager);
    const auto handler = [guarded](const std::string &action) {
        return [guarded, action](const httplib::Request &req, httplib::Response &res) {
            Reply reply = failure(503, QStringLiteral("TCP control is unavailable"));
            if (guarded) {
                const QString serverId = QString::fromStdString(req.get_param_value("server_id"));
                const QString commandId = QString::fromStdString(req.get_param_value("command_id"));
                const bool hasWidth = req.has_param("charsPerItem");
                const QString widthParameter = QString::fromStdString(req.get_param_value("charsPerItem"));
                const auto pending = std::make_shared<PendingReply>();
                pending->reply = reply;
                const auto execute = [guarded, action, serverId, commandId, hasWidth, widthParameter, pending] {
                    auto &reply = pending->reply;
                    auto *manager = guarded.data();
                    if (!manager) return;
                    if (action == "servers") {
                        reply = {200, {{QStringLiteral("servers"), QJsonArray::fromVariantList(manager->servers())}}};
                        return;
                    }
                    if (action == "commands") {
                        if (!serverId.isEmpty()) {
                            bool found = false;
                            for (const auto &entry : manager->servers())
                                if (entry.toMap().value(QStringLiteral("id")).toString() == serverId) { found = true; break; }
                            if (!found) { reply = failure(404, QStringLiteral("TCP server does not exist")); return; }
                        }
                        QJsonArray commands;
                        for (const auto &entry : manager->commands()) {
                            const auto command = entry.toMap();
                            if (serverId.isEmpty() || command.value(QStringLiteral("serverId")).toString() == serverId)
                                commands.append(QJsonObject::fromVariantMap(command));
                        }
                        reply = {200, {{QStringLiteral("commands"), commands}}};
                        return;
                    }
                    if (action == "command") {
                        if (commandId.isEmpty() || !hasWidth) {
                            reply = failure(400, QStringLiteral("Expected command_id and charsPerItem"));
                            return;
                        }
                        if (manager->command(commandId).isEmpty()) {
                            reply = failure(404, QStringLiteral("TCP command does not exist"));
                            return;
                        }
                        bool valid = false;
                        const int width = widthParameter.toInt(&valid);
                        if (!valid || width < 0 || width > 4096) {
                            reply = failure(400, QStringLiteral("charsPerItem must be an integer from 0 to 4096"));
                            return;
                        }
                        if (!manager->setCommandCharsPerItem(commandId, width)) reply = failure(409, manager->lastError());
                        else reply = {200, {{QStringLiteral("success"), true},
                            {QStringLiteral("command"), QJsonObject::fromVariantMap(manager->command(commandId))}}};
                        return;
                    }
                    if (serverId.isEmpty()) {
                        reply = failure(400, QStringLiteral("Expected server_id"));
                        return;
                    }
                    bool found = false;
                    for (const auto &entry : manager->servers())
                        if (entry.toMap().value(QStringLiteral("id")).toString() == serverId) { found = true; break; }
                    if (!found) { reply = failure(404, QStringLiteral("TCP server does not exist")); return; }
                    if (action == "connect" || action == "disconnect") {
                        const bool accepted = action == "connect" ? manager->connectServer(serverId) : manager->disconnectServer(serverId);
                        if (!accepted) reply = failure(409, manager->lastError());
                        else reply = {action == "connect" ? 202 : 200, {{QStringLiteral("success"), true},
                            {QStringLiteral("server_id"), serverId}}};
                        return;
                    }
                    if (commandId == QStringLiteral("all")) {
                        const auto result = manager->triggerAll(serverId);
                        reply = {result.value(QStringLiteral("success")).toBool() ? 200 : 409, QJsonObject::fromVariantMap(result)};
                    } else {
                        const auto command = manager->command(commandId);
                        if (commandId.isEmpty()) reply = failure(400, QStringLiteral("Expected command_id (or all)"));
                        else if (command.isEmpty() || command.value(QStringLiteral("serverId")).toString() != serverId)
                            reply = failure(404, QStringLiteral("TCP command does not exist on this server"));
                        else if (!manager->triggerCommand(commandId)) reply = failure(409, manager->lastError());
                        else reply = {200, {{QStringLiteral("success"), true}}};
                    }
                };
                if (QThread::currentThread() == guarded->thread()) {
                    execute();
                    reply = pending->reply;
                } else {
                    auto completed = pending->completed.get_future();
                    QMetaObject::invokeMethod(guarded.data(), [execute, pending] {
                        if (pending->expired.load()) return;
                        execute();
                        pending->completed.set_value(pending->reply);
                    }, Qt::QueuedConnection);
                    // Shutdown may stop the Qt loop while HTTP workers are still alive.
                    // Own request data and bound the wait so server shutdown can complete.
                    if (completed.wait_for(std::chrono::seconds(5)) == std::future_status::ready) reply = completed.get();
                    else {
                        pending->expired.store(true);
                        reply = failure(503, QStringLiteral("TCP control request timed out"));
                    }
                }
            }
            res.status = reply.status;
            res.set_header("Cache-Control", "no-store");
            res.set_content(QJsonDocument(reply.body).toJson(QJsonDocument::Compact).toStdString(), "application/json");
        };
    };
    server.Get("/tcp/servers", handler("servers"));
    server.Get("/tcp/commands", handler("commands"));
    const TcpPostRegistrar post = registerPost ? registerPost : [&server](const std::string &path, auto callback) { server.Post(path, std::move(callback)); };
    post("/tcp/connect", handler("connect"));
    post("/tcp/disconnect", handler("disconnect"));
    post("/tcp/trigger", handler("trigger"));
    post("/tcp/command", handler("command"));
}
