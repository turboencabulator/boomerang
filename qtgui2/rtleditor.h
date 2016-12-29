/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef RTLEDITOR_H
#define RTLEDITOR_H

#include <QtCore/QString>
#include <QtGui/QTextEdit>

class Decompiler;

class RTLEditor : public QTextEdit {
	Q_OBJECT

public:
	RTLEditor(Decompiler *decompiler, const QString &name);

public slots:
	void updateContents();

protected:
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);

private:
	Decompiler *decompiler;
	QString name;
};

#endif
