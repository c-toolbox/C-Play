/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "slidesmodel.h"
#include "layersmodel.h"
#include "locationsettings.h"
#include "presentationsettings.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SlidesModel::SlidesModel(QObject *parent)
    : QAbstractListModel(parent),
      m_masterSlide(new LayersModel(this)),
      m_needsSync(false),
      m_slidesName(QStringLiteral("")),
      m_slidesPath(QStringLiteral("")) {
    m_masterSlide->setLayersName(QStringLiteral("Master"));
}

SlidesModel::~SlidesModel() {
    for (auto s : m_slides)
        delete s;
    m_slides.clear();
    delete m_masterSlide;
}

int SlidesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_slides.size();
}

QVariant SlidesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_slides.empty())
        return QVariant();

    LayersModel *slideItem = m_slides.at(index.row());

    switch (role) {
    case NameRole:
        return QVariant(slideItem->getLayersName());
    case PathRole:
        return QVariant(slideItem->getLayersPath());
    case LayersRole:
        return QVariant(slideItem->numberOfLayers());
    case VisibilityRole:
        return QVariant(slideItem->getLayersVisibility());
    }

    return QVariant();
}

QHash<int, QByteArray> SlidesModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "filepath";
    roles[LayersRole] = "layers";
    roles[VisibilityRole] = "visibility";
    return roles;
}

int SlidesModel::numberOfSlides() {
    return m_slides.size();
}

bool SlidesModel::needsSync() {
    return m_needsSync;
}

void SlidesModel::setNeedsSync(bool value) {
    m_needsSync = value;
    Q_EMIT needsSyncChanged();
}

void SlidesModel::setHasSynced() {
    setNeedsSync(false);
}

int SlidesModel::selectedSlideIdx() {
    return m_selectedSlideIdx;
}

void SlidesModel::setSelectedSlideIdx(int value) {
    m_previousSelectedSlideIdx = m_selectedSlideIdx;
    m_selectedSlideIdx = value;
    Q_EMIT selectedSlideChanged();
}

int SlidesModel::previousSlideIdx() {
    return m_previousSelectedSlideIdx;
}

int SlidesModel::nextSlideIdx() {
    if (m_selectedSlideIdx + 1 < m_slides.size())
        return m_selectedSlideIdx + 1;
    else if (m_slides.empty())
        return -1;
    else
        return 0;
}

int SlidesModel::triggeredSlideIdx() {
    return m_triggeredSlideIdx;
}

void SlidesModel::setTriggeredSlideIdx(int value) {
    m_previousTriggeredSlideIdx = m_triggeredSlideIdx;
    m_triggeredSlideIdx = value;
    Q_EMIT triggeredSlideChanged();
}

int SlidesModel::triggeredSlideVisibility() {
    if (m_triggeredSlideIdx >= 0 && m_triggeredSlideIdx < m_slides.size())
        return slide(m_triggeredSlideIdx)->getLayersVisibility();
    else
        return 0;
}

void SlidesModel::setTriggeredSlideVisibility(int value) {
    // Propogate down
    if (m_triggeredSlideIdx >= 0 && m_triggeredSlideIdx < m_slides.size()) {
        slide(m_triggeredSlideIdx)->setLayersVisibility(value);
        updateSlide(m_triggeredSlideIdx);
    }

    // If previous slide is high
    if (m_previousTriggeredSlideIdx != m_triggeredSlideIdx && m_previousTriggeredSlideIdx >= 0 && m_previousTriggeredSlideIdx < m_slides.size()) {
        if (slide(m_previousTriggeredSlideIdx)->getLayersVisibility() > 0) {
            slide(m_previousTriggeredSlideIdx)->setLayersVisibility(100 - value);
            updateSlide(m_previousTriggeredSlideIdx);
        }
    }

    setSlidesNeedsSave(true);

    Q_EMIT triggeredSlideVisibilityChanged();
}

int SlidesModel::previousTriggeredIdx() {
    return m_previousTriggeredSlideIdx;
}

LayersModel *SlidesModel::masterSlide() {
    return m_masterSlide;
}

LayersModel *SlidesModel::slide(int i) {
    if (i == -1)
        return masterSlide();
    else if (i >= 0 && m_slides.size() > i)
        return m_slides[i];
    else
        return nullptr;
}

LayersModel *SlidesModel::selectedSlide() {
    return slide(m_selectedSlideIdx);
}

int SlidesModel::addSlide() {
    beginInsertRows(QModelIndex(), m_slides.size(), m_slides.size());
    m_slides.push_back(new LayersModel(this));
    setSlidesNeedsSave(true);
    m_needsSync = true;
    endInsertRows();

    return m_slides.size() - 1;
}

void SlidesModel::removeSlide(int i) {
    if (i < 0 || i >= m_slides.size())
        return;

    beginRemoveRows(QModelIndex(), i, i);
    m_slides.removeAt(i);

    if (m_triggeredSlideIdx == i)
        m_triggeredSlideIdx = -1;
    else if (m_triggeredSlideIdx > i)
        m_triggeredSlideIdx -= 1;

    if (m_previousTriggeredSlideIdx == i)
        m_previousTriggeredSlideIdx = -2;
    else if (m_previousTriggeredSlideIdx > i)
        m_previousTriggeredSlideIdx -= 1;

    if (m_previousSelectedSlideIdx == i)
        m_previousSelectedSlideIdx = -2;
    else if (m_previousSelectedSlideIdx > i)
        m_previousSelectedSlideIdx -= 1;

    if (m_selectedSlideIdx == i)
        m_selectedSlideIdx = -1;

    endRemoveRows();
    setSlidesNeedsSave(true);
    m_needsSync = true;
}

void SlidesModel::moveSlideUp(int i) {
    if (i == 0)
        return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_slides.move(i, i - 1);
    m_previousSelectedSlideIdx = i - 1;
    endMoveRows();
    setSlidesNeedsSave(true);
    m_needsSync = true;
}

void SlidesModel::moveSlideDown(int i) {
    if (i == (m_slides.size() - 1))
        return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_slides.move(i, i + 1);
    m_previousSelectedSlideIdx = i + 1;
    endMoveRows();
    setSlidesNeedsSave(true);
    m_needsSync = true;
}

void SlidesModel::updateSlide(int i) {
    if (i >= 0 && i < m_slides.size()) {
        Q_EMIT dataChanged(index(i, 0), index(i, 0));
        setSlidesNeedsSave(true);
    }
}

void SlidesModel::updateSelectedSlide() {
    if (m_selectedSlideIdx >= 0 && m_selectedSlideIdx < m_slides.size())
        updateSlide(m_selectedSlideIdx);
}

void SlidesModel::clearSlides() {
    beginRemoveRows(QModelIndex(), 0, m_slides.size() - 1);
    m_slides.clear();
    m_masterSlide->clearLayers();
    endRemoveRows();
    setSlidesName(QStringLiteral(""));
    setSlidesPath(QStringLiteral(""));
    setSlidesNeedsSave(false);
    m_needsSync = true;
}

void SlidesModel::setSlidesNeedsSave(bool value) {
    m_slidesNeedsSave = value;
    Q_EMIT slidesNeedsSaveChanged();
}

bool SlidesModel::getSlidesNeedsSave() {
    return m_slidesNeedsSave;
}

void SlidesModel::setSlidesName(QString name) {
    m_slidesName = name;
    Q_EMIT slidesNameChanged();
}

QString SlidesModel::getSlidesName() const {
    return m_slidesName;
}

void SlidesModel::setSlidesPath(QString path) {
    m_slidesPath = path;
}

QString SlidesModel::getSlidesPath() const {
    return m_slidesPath;
}

QUrl SlidesModel::getSlidesPathAsURL() const {
    return QUrl(QStringLiteral("file:///") + m_slidesPath);
}

QString SlidesModel::makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void SlidesModel::loadFromJSONFile(const QString &path) {
    QString fileToOpen = path;
    fileToOpen.replace(QStringLiteral("file:///"), QStringLiteral(""));

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());

    fileToOpen = m_masterSlide->checkAndCorrectPath(fileToOpen, pathsToConsider);

    QFileInfo jsonFileInfo(fileToOpen);
    if (!jsonFileInfo.exists()) {
        qDebug() << QStringLiteral("C-play presentation ") << fileToOpen << QStringLiteral(" did not exist.");
        return;
    }

    QFile f(fileToOpen);
    f.open(QIODevice::ReadOnly);
    QByteArray fileContent = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileContent);
    if (doc.isNull()) {
        qDebug() << QStringLiteral("Parsing C-play presentation failed: ") << fileToOpen;
        return;
    }

    QJsonObject obj = doc.object();

    QStringList fileSearchPaths;
    fileSearchPaths.append(jsonFileInfo.absoluteDir().absolutePath());
    fileSearchPaths.append(LocationSettings::cPlayMediaLocation());
    fileSearchPaths.append(LocationSettings::cPlayFileLocation());
    fileSearchPaths.append(LocationSettings::univiewVideoLocation());

    if (obj.contains(QStringLiteral("slides"))) {
        clearSlides();
        QJsonValue value = obj.value(QStringLiteral("slides"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            QJsonObject o = v.toObject();
            int idx = addSlide();
            m_slides[idx]->decodeFromJSON(o, fileSearchPaths);
        }
    }

    if (obj.contains(QStringLiteral("master"))) {
        m_masterSlide->clearLayers();
        QJsonValue value = obj.value(QStringLiteral("master"));
        QJsonObject o = value.toObject();
        m_masterSlide->decodeFromJSON(o, fileSearchPaths);
    }

    setSelectedSlideIdx(-1);
    setSlidesPath(jsonFileInfo.absoluteFilePath());
    setSlidesName(jsonFileInfo.baseName());
    setSlidesNeedsSave(false);
    m_needsSync = true;
}

void SlidesModel::saveAsJSONFile(const QString &path) {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());

    QJsonObject masterSlideData;
    m_masterSlide->encodeToJSON(masterSlideData, pathsToConsider);
    obj.insert(QStringLiteral("master"), masterSlideData);

    QJsonArray slidesArray;
    for (auto slide : m_slides) {
        QJsonObject slideData;
        slide->encodeToJSON(slideData, pathsToConsider);
        slidesArray.push_back(QJsonValue(slideData));
    }
    obj.insert(QString(QStringLiteral("slides")), QJsonValue(slidesArray));

    doc.setObject(obj);
    QFile jsonFile(fileToSave);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(doc.toJson());
    jsonFile.close();

    QFileInfo fileInfo(jsonFile);
    setSlidesName(fileInfo.baseName());
    setSlidesPath(fileToSave);

    setSlidesNeedsSave(false);
}