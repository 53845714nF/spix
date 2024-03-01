//
// Created by sebastian on 28.02.24.
//

#pragma once
#include <QObject>
#include <QMouseEvent>
#include <QKeyEvent>

namespace spix {

class QtEventFilter : public QObject {
	Q_OBJECT
public:
    QtEventFilter(QObject* parent);

signals:
	void pickerModeEntered(QKeyEvent* event);
	void pickerModeExited(QKeyEvent* event);
	void pickClick(QMouseEvent* event);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

} // namespace spix