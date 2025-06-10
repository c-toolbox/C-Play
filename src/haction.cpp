/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "haction.h"

#include <QKeySequence>

HAction::HAction(QObject *parent) : QAction(parent) {
}

QString HAction::actionName() {
    return objectName();
}

QString HAction::shortcutName() {
    return shortcut().toString(QKeySequence::NativeText);
}

QString HAction::alternateName() {
    if (shortcuts().size() > 1)
        return shortcuts()[1].toString(QKeySequence::NativeText);
    else
        return QStringLiteral("");
}

QString HAction::iconName() {
    return icon().name();
}
