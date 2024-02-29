/***
 * Copyright (C) Falko Axmann. All rights reserved.
 * Licensed under the MIT license.
 * See LICENSE.txt file in the project root for full license information.
 ****/

#include "QtScene.h"

#include <Scene/Qt/QtItem.h>
#include <Scene/Qt/QtItemTools.h>
#include <Spix/Data/ItemPath.h>

#include <QGuiApplication>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickItem>
#include <QByteArray>
#include <QBuffer>
#include "QtEventFilter.cpp"

namespace {

/**
 * @brief Return root element from QML
 * @param name of the root Element
 * @return the root element 
 */
QQuickWindow* getQQuickWindowWithName(const std::string& name)
{
    QString qtName = QString::fromStdString(name);
    QQuickWindow* foundWindow = nullptr;

    auto windows = QGuiApplication::topLevelWindows();
    for (const auto& window : windows) {
        QQuickWindow* qquickWindow = qobject_cast<QQuickWindow*>(window);
        if (qquickWindow && (spix::qt::GetObjectName(qquickWindow) == qtName)) {
            foundWindow = qquickWindow;
            break;
        }
    }

    return foundWindow;
}

/**
 * @brief Return QML-Item from Path with Root Element
 *
 * @param path is ItemPath 
 * @param root ist the root Element
 * @return Qml Item 
 */
QQuickItem* getQQuickItemWithRoot(const spix::ItemPath& path, QObject* root)
{
    if (path.length() == 0) {
        return nullptr;
    }
    if (!root) {
        return nullptr;
    }

    auto rootClassName = root->metaObject()->className();
    auto itemName = path.rootComponent();
    QQuickItem* subItem = nullptr;

    if (itemName.compare(0, 1, ".") == 0) {
        auto propertyName = itemName.substr(1);
        QVariant propertyValue = root->property(propertyName.c_str());
        if (propertyValue.isValid()) {
            subItem = propertyValue.value<QQuickItem*>();
        }

    } else if (itemName.compare(0, 1, "\"") == 0) {
        auto propertyName = itemName.substr(1);
        QVariant propertyValue = root->property(propertyName.c_str());
        
        size_t found = itemName.find("\"");
        auto searchText = itemName.substr(found +1, itemName.length() -2);
        subItem = spix::qt::FindChildItem<QQuickItem*>(root, itemName.c_str(), QString::fromStdString(searchText), {});

    } else if (itemName.compare(0, 1, "#") == 0) {
        auto propertyName = itemName.substr(1);
        QVariant propertyValue = root->property(propertyName.c_str());
        
        size_t found = itemName.find("#");
        auto type = QString::fromStdString(itemName.substr(found +1));
        subItem = spix::qt::FindChildItem<QQuickItem*>(root, itemName.c_str(), {}, type);
    } else if (rootClassName == spix::qt::repeater_class_name) {
        QQuickItem* repeater = static_cast<QQuickItem*>(root);
        subItem = spix::qt::RepeaterChildWithName(repeater, QString::fromStdString(itemName));
    } else {
        subItem = spix::qt::FindChildItem<QQuickItem*>(root, itemName.c_str());
    }

    if (path.length() == 1) {
        return subItem;
    }

    return getQQuickItemWithRoot(path.subPath(1), subItem);
}

/**
 * @brief Return QML-Item for a Path 
 * @param path to the QML Item
 * @return QML Item 
 */
QQuickItem* getQQuickItemAtPath(const spix::ItemPath& path)
{   
    auto windowName = path.rootComponent();
    QQuickWindow* itemWindow = getQQuickWindowWithName(windowName);
    QQuickItem* item = nullptr;

    if (!itemWindow) {
        return nullptr;
    }

    if (path.length() > 1) {
        item = getQQuickItemWithRoot(path.subPath(1), itemWindow);
    } else {
        item = itemWindow->contentItem();
    }
    
    return item;
}

} // namespace

namespace spix {

/**
Create a QtScene with an EventFilter
**/
QtScene::QtScene(){
    m_filter = new QtEventFilter(qGuiApp);

    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, qGuiApp, [this](QWindow* window){
         if (m_eventFilterInstalled == false) {
            m_eventFilterInstalled = true;
            window->installEventFilter(m_filter);

			QObject::connect(m_filter, &QtEventFilter::pickerModeEntered, m_filter, [](){
				qDebug() << "Enter Curser Mode";
				QGuiApplication::setOverrideCursor(QCursor(Qt::CrossCursor));
			});

			QObject::connect(m_filter, &QtEventFilter::pickerModeExited, m_filter, [](){
				QGuiApplication::restoreOverrideCursor();
			});

			auto quickWindow = qobject_cast<QQuickWindow* >(window);
			QObject::connect(m_filter, &QtEventFilter::pickClick, m_filter, [this, quickWindow](QMouseEvent* event){
				qDebug() << "Got pickClick: " << event;
				int bestCanidate = -1;
				bool parentIsGoodCandidate = true;
				auto objects = recursiveItemsAt(quickWindow->contentItem(), event->pos(), bestCanidate, parentIsGoodCandidate);
				qDebug() << "Object gefunden : " << objects;
				if (objects.size() == 1) {
					auto quickItem = qobject_cast<QQuickItem* >(objects[0]);
					quickItem->setOpacity(0.5);
				}
			});
        }
    });
}

QtScene::~QtScene(){
    delete m_filter;
}

std::unique_ptr<Item> QtScene::itemAtPath(const ItemPath& path)
{
    auto windowName = path.rootComponent();
    QQuickWindow* itemWindow = getQQuickWindowWithName(windowName);

    if (!itemWindow || !itemWindow->contentItem()) {
        return {};
    }
    if (path.length() <= 1) {
        return std::make_unique<QtItem>(itemWindow);
    }

    auto item = getQQuickItemWithRoot(path.subPath(1), itemWindow);

    if (!item) {
        return {};
    }
    return std::make_unique<QtItem>(item);
}

Events& QtScene::events()
{
    return m_events;
}

void QtScene::takeScreenshot(const ItemPath& targetItem, const std::string& filePath)
{
    auto item = getQQuickItemAtPath(targetItem);
    if (!item) {
        return;
    }

    // take screenshot of the full window
    auto windowImage = item->window()->grabWindow();

    // get the rect of the item in window space in pixels, account for the device pixel ratio
    QRectF imageCropRectItemSpace {0, 0, item->width(), item->height()};
    auto imageCropRectF = item->mapRectToScene(imageCropRectItemSpace);
    QRect imageCropRect(imageCropRectF.x() * windowImage.devicePixelRatio(),
        imageCropRectF.y() * windowImage.devicePixelRatio(), imageCropRectF.width() * windowImage.devicePixelRatio(),
        imageCropRectF.height() * windowImage.devicePixelRatio());

    // crop the window image to the item rect
    auto image = windowImage.copy(imageCropRect);
    image.save(QString::fromStdString(filePath));
}

std::string QtScene::takeScreenshotRemote(const ItemPath& targetItem)
{
    auto item = getQQuickItemAtPath(targetItem);
    if (!item) {
        return "";
    }

    // take screenshot of the full window
    auto windowImage = item->window()->grabWindow();

    // get the rect of the item in window space in pixels, account for the device pixel ratio
    QRectF imageCropRectItemSpace {0, 0, item->width(), item->height()};
    auto imageCropRectF = item->mapRectToScene(imageCropRectItemSpace);
    QRect imageCropRect(imageCropRectF.x() * windowImage.devicePixelRatio(),
        imageCropRectF.y() * windowImage.devicePixelRatio(), imageCropRectF.width() * windowImage.devicePixelRatio(),
        imageCropRectF.height() * windowImage.devicePixelRatio());

    // crop the window image to the item rect
    auto image = windowImage.copy(imageCropRect);
    
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();
    
    return byteArray.toBase64().toStdString();
}

bool QtScene::itemHasContents(QQuickItem *item)
{
    return item->flags().testFlag(QQuickItem::ItemHasContents);
}

bool QtScene::isGoodCandidateItem(QQuickItem *item, bool ignoreItemHasContents = false)
{
    return !(!item->isVisible() || qFuzzyCompare(item->opacity() + qreal(1.0), qreal(1.0)) || (!ignoreItemHasContents && !itemHasContents(item)));
}
/**
	Search for best matching Object on the Position.
**/
ObjectIds QtScene::recursiveItemsAt(QQuickItem *parent, const QPointF &pos, int &bestCandidate, bool parentIsGoodCandidate)
{
	 Q_ASSERT(parent); // nulll check
     ObjectIds objects;

	auto printName = QString("");
	if (spix::qt::GetObjectName(parent) != "" ){
		printName = spix::qt::GetObjectName(parent) + "/";
	} else {
		printName = "#" + spix::qt::TypeByObject(parent) + "/";
	}
	qDebug() << "Parent: "<< printName;

	bestCandidate = -1;
    if (parentIsGoodCandidate) {
        // inherit the parent item opacity when looking for a good candidate item
        // i.e. QQuickItem::isVisible is taking the parent into account already, but
        // the opacity doesn't - we have to do this manually
        // Yet we have to ignore ItemHasContents apparently, as the QQuickRootItem
        // at least seems to not have this flag set.
        parentIsGoodCandidate = isGoodCandidateItem(parent, true);
    }

	// sorting based on z positon
    auto childItems = parent->childItems();
    std::stable_sort(childItems.begin(), childItems.end(),
                     [](QQuickItem *lhs, QQuickItem *rhs) { return lhs->z() < rhs->z(); });

    for (int i = childItems.size() - 1; i >= 0; --i) { // backwards to match z order
        const auto child = childItems.at(i);
        // position of child
        const auto requestedPoint = parent->mapToItem(child, pos);

        if (!child->childItems().isEmpty() && (child->contains(requestedPoint) || child->childrenRect().contains(requestedPoint))) {
            const int count = objects.count();
            int bc; // possibly better candidate among subChildren

            objects << recursiveItemsAt(child, requestedPoint, bc, parentIsGoodCandidate);

            if (bestCandidate == -1 && parentIsGoodCandidate && bc != -1) {
                bestCandidate = count + bc;
            }
        }

        if (child->contains(requestedPoint)) {
            if (bestCandidate == -1 && parentIsGoodCandidate && isGoodCandidateItem(child)) {
                bestCandidate = objects.count();
            }
            objects << child;
        }

        if (bestCandidate != -1) {
            break;
        }
    }

    if (bestCandidate == -1 && parentIsGoodCandidate && itemHasContents(parent)) {
        bestCandidate = objects.count();
    }

    objects << parent;

    if (bestCandidate != -1) {
        objects = ObjectIds() << objects[bestCandidate];
        bestCandidate = 0;
    }

    return objects;
}

} // namespace spix
