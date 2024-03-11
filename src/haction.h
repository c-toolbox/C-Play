/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HACTION_H
#define HACTION_H

#include <QtWidgets/qaction.h>

class HAction : public QAction
{
    Q_OBJECT
public:
    explicit HAction(QObject *parent = nullptr);

public slots:
    QString shortcutName();
    QString iconName();

};

#endif // HACTION_H
