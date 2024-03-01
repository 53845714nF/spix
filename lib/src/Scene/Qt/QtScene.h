/***
 * Copyright (C) Falko Axmann. All rights reserved.
 * Licensed under the MIT license.
 * See LICENSE.txt file in the project root for full license information.
 ****/

#pragma once

#include <Scene/Qt/QtEvents.h>
#include <QByteArray>
#include <Scene/Scene.h>
#include <QQuickItem>

#include <map>
#include <string>


class QQuickWindow;

namespace spix {

class QtEventFilter;
class ItemPath;

using ObjectIds = QVector<QObject* >;

class QtScene : public Scene {
public:
     QtScene();
    ~QtScene();
    // Request objects
    std::unique_ptr<Item> itemAtPath(const ItemPath& path) override;

    // Events
    Events& events() override;

    // Tasks
    void takeScreenshot(const ItemPath& targetItem, const std::string& filePath) override;
    std::string takeScreenshotRemote(const ItemPath& targetItem);
	ObjectIds recursiveItemsAt(QQuickItem *parent, const QPointF &pos, int &bestCandidate, bool parentIsGoodCandidate, QString& path);

private:
    QtEvents m_events;
    bool m_eventFilterInstalled = false;
    QtEventFilter* m_filter;
	bool itemHasContents(QQuickItem *item);
	bool isGoodCandidateItem(QQuickItem *item, bool ignoreItemHasContents);
};

} // namespace spix
