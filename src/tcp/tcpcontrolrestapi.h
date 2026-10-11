/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TCPCONTROLRESTAPI_H
#define TCPCONTROLRESTAPI_H
#include <functional>
#include <string>
namespace httplib { class Server; struct Request; struct Response; }
using TcpPostRegistrar = std::function<void(const std::string &, std::function<void(const httplib::Request &, httplib::Response &)>)>;
class TcpControlManager;

// HTTP workers marshal all manager operations onto its Qt event-loop thread.
void registerTcpControlRoutes(httplib::Server &server, TcpControlManager *manager, const TcpPostRegistrar &registerPost = {});
#endif
