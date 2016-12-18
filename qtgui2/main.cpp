#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mainwindow.h"

#include "gc.h"

#include <QApplication>

void init_dfa();         // Prototypes for
void init_sslparser();   // various initialisation functions
void init_basicblock();  // for garbage collection safety

int main(int argc, char *argv[])
{
	init_dfa();
	init_sslparser();
	init_basicblock();

	QApplication app(argc, argv);
	MainWindow mainWindow;
	mainWindow.show();
	return app.exec();
}
