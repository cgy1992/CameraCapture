#pragma once

#include <QMainWindow>
#include "ui_mainwindow.h"
#include "recorder.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = 0);

private slots:
	void updateDeviceList();
	void toggleRecord();

private:
	Ui::MainWindow ui;
	Recorder recorder;
	QCamera * selectedCamera;
};
