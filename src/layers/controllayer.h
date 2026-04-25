/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CONTROLLAYER_H
#define CONTROLLAYER_H

#include <layers/baselayer.h>
#include <functional>

class ControlLayer : public BaseLayer {
public:
    using DispatchCallback = std::function<void(const std::string&, const std::string&)>;

    ControlLayer();
    ~ControlLayer() = default;

    void initialize() override;
    bool existOnMasterOnly() const override;
    bool ready() const override;
    bool hasTexture() const override;

    void start() override;
    void stop() override;

    std::string operation() const;
    void setOperation(const std::string& op);

    std::string parameter() const;
    void setParameter(const std::string& param);

    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

    static void setDispatchCallback(DispatchCallback cb);

private:
    std::string m_operation;
    std::string m_parameter;

    static DispatchCallback s_dispatchCallback;
};

#endif // CONTROLLAYER_H
