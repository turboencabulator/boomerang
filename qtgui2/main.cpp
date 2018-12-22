/**
 * \file
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mainwindow.h"

#include <QApplication>

#ifdef GARBAGE_COLLECTOR
// Prototypes for various initialisation functions for garbage collection safety
void init_dfa();
void init_basicblock();
#endif

int
main(int argc, char *argv[])
{
#ifdef GARBAGE_COLLECTOR
	init_dfa();
	init_basicblock();
#endif

	QApplication app(argc, argv);
	MainWindow mainWindow;
	mainWindow.show();
	return app.exec();
}
