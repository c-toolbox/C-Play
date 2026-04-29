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

class HttpClientModel;

class RestLayer : public BaseLayer {
public:
    enum RequestMethod {
        GET = 0,
        POST,
        PUT,
        DELETE_METHOD
    };

    RestLayer();
    ~RestLayer() = default;

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

    std::string requestBody() const;
    void setRequestBody(const std::string& body);

    std::string contentType() const;
    void setContentType(const std::string& ct);

    void setHttpClientModel(HttpClientModel* model);

    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

private:
    std::string m_url;
    int m_method = GET;
    std::string m_requestBody;
    std::string m_contentType = "application/json";

    HttpClientModel* m_httpClientModel = nullptr;
};

#endif // RESTLAYER_H
