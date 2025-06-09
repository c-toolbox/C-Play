/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACK_H
#define TRACK_H

#include <string>

class Track {
public:
    Track();
    ~Track();

    std::string lang() const;
    void setLang(const std::string &lang);

    std::string title() const;
    void setTitle(const std::string &title);

    std::string codec() const;
    void setCodec(const std::string &codec);

    __int64 id() const;
    void setId(const __int64 &id);

    __int64 ffIndex() const;
    void setFfIndex(const __int64 &ffIndex);

    __int64 srcId() const;
    void setSrcId(const __int64 &srcId);

    bool dependent() const;
    void setDependent(bool dependent);

    bool external() const;
    void setExternal(bool external);

    bool forced() const;
    void setForced(bool forced);

    bool defaut() const;
    void setDefaut(bool defaut);

    std::string type() const;
    void setType(const std::string &type);

    int index() const;
    void setIndex(int index);

private:
    std::string m_lang;
    std::string m_title;
    std::string m_shortTitle;
    std::string m_codec;
    std::string m_type;
    __int64 m_id{};
    __int64 m_ffIndex{};
    __int64 m_srcId{};
    bool m_defaut{};
    bool m_dependent{};
    bool m_external{};
    bool m_forced{};
    int m_index{};
};

#endif // TRACK_H
