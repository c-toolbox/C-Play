/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "track.h"
#include <filesystem>

Track::Track() {}

Track::~Track() {};

std::string Track::lang() const {
    return m_lang;
}

void Track::setLang(const std::string &lang) {
    m_lang = lang;
}

std::string Track::title() const {
    return m_title;
}

void Track::setTitle(const std::string &title) {
    m_title = title;
}

std::string Track::codec() const {
    return m_codec;
}

void Track::setCodec(const std::string &codec) {
    m_codec = codec;
}

__int64 Track::id() const {
    return m_id;
}

void Track::setId(const __int64 &id) {
    m_id = id;
}

__int64 Track::ffIndex() const {
    return m_ffIndex;
}

void Track::setFfIndex(const __int64 &ffIndex) {
    m_ffIndex = ffIndex;
}

__int64 Track::srcId() const {
    return m_srcId;
}

void Track::setSrcId(const __int64 &srcId) {
    m_srcId = srcId;
}

bool Track::dependent() const {
    return m_dependent;
}

void Track::setDependent(bool dependent) {
    m_dependent = dependent;
}

bool Track::external() const {
    return m_external;
}

void Track::setExternal(bool external) {
    m_external = external;
}

bool Track::forced() const {
    return m_forced;
}

void Track::setForced(bool forced) {
    m_forced = forced;
}

bool Track::defaut() const {
    return m_defaut;
}

void Track::setDefaut(bool defaut) {
    m_defaut = defaut;
}

std::string Track::type() const {
    return m_type;
}

void Track::setType(const std::string &type) {
    m_type = type;
}

int Track::index() const {
    return m_index;
}

void Track::setIndex(int index) {
    m_index = index;
}
