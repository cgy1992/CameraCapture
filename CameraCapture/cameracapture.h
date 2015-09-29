#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QtWidgets/QMainWindow>
#include "ui_cameracapture.h"

#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <Dbt.h>
#include <shlwapi.h>
#include <comdef.h>

class CameraCapture : public QMainWindow
{
	Q_OBJECT

public:
	CameraCapture(QWidget *parent = 0);
	~CameraCapture();

private:
	Ui::CameraCaptureClass ui;
	IMFSinkWriter * writer = nullptr;
};

#endif // CAMERACAPTURE_H
