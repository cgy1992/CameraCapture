#pragma once

#include <memory>
#include <QMainWindow>
#include <QCameraInfo>
#include <QAudioDeviceInfo>

#include "ui_mainwindow.h"
#include "recorder.h"
#include "grayscalefilter.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = 0);

private slots:
	void toggleRecord();
	void selectCamera(const QCameraInfo & cameraInfo);
	void selectAudioDevice(const QAudioDeviceInfo & audioDeviceInfo);

private:
	Ui::MainWindow ui;
	Recorder recorder;
	std::unique_ptr<QCamera> selectedCamera;
	std::unique_ptr<GrayscaleFilter> grayscaleFilter;
	QList<QCameraInfo> cameraInfoList;
	QList<QAudioDeviceInfo> micList;
};
