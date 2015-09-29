#include "cameracapture.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	CameraCapture w;
	w.show();
	return a.exec();
}
