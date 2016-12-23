#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mainwindow.h"

#include <QApplication>

#ifdef GARBAGE_COLLECTOR
// Prototypes for various initialisation functions for garbage collection safety
void init_dfa();
void init_sslparser();
void init_basicblock();
#endif

int main(int argc, char *argv[])
{
#ifdef GARBAGE_COLLECTOR
	init_dfa();
	init_sslparser();
	init_basicblock();
#endif

	QApplication app(argc, argv);
	MainWindow mainWindow;
	mainWindow.show();
	return app.exec();
}
