/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RESTLAYER_H
#define RESTLAYER_H

#include <layers/baselayer.h>
#include <string>
#include <functional>
#include <QObject>
#include <QThread>

class HttpClientModel;
class HttpRequestWorker;
class WwsRequestWorker;

class RestLayer : public BaseLayer {
public:
    enum RequestMethod {
        GET = 0,
        POST,
        PUT,
        DELETE_METHOD,
        WS,
        WSS
    };

    RestLayer();
    ~RestLayer();

    void cleanup() override;
    void initialize() override;
    bool existOnMasterOnly() const override;
    bool ready() const override;
    bool hasTexture() const override;

    void start() override;
    void stop() override;

    std::string url() const;
    void setUrl(const std::string& u);

    int method() const;
    void setMethod(int m);

    std::string parameters() const;
    void setParameters(const std::string& params);

    bool ignoreStatus() const;
    void setIgnoreStatus(bool ignore);

    void setHttpClientModel(HttpClientModel* model);

    // Callback invoked on the main thread when the request finishes.
    // The int parameter is the layer status: 2=success, 0=failure.
    using StatusCallback = std::function<void(int)>;
    void setStatusCallback(StatusCallback cb);

    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

private:
    void onRequestFinished(int statusCode, const QString &responseBody, const QString &error);
    bool useWebSocket() const;

    std::string m_url;
    int m_method = GET;
    std::string m_parameters;
    bool m_ignoreStatus = false;

    HttpClientModel* m_httpClientModel = nullptr;
    StatusCallback m_statusCallback;

    QThread* m_workerThread = nullptr;
    HttpRequestWorker* m_worker = nullptr;
    WwsRequestWorker* m_wwsWorker = nullptr;
};

#endif // RESTLAYER_H
