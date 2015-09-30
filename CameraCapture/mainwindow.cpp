#include "mainwindow.h"

#include <QCamera>
#include <QCameraInfo>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QAudioDeviceInfo>
#include <QAudio>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	updateDeviceList();

	for(const QCameraInfo & cameraInfo : QCameraInfo::availableCameras())
	{
		selectedCamera = new QCamera(cameraInfo);
		selectedCamera->setCaptureMode(QCamera::CaptureStillImage);
		selectedCamera->setViewfinder(ui.viewfinder);
		selectedCamera->start();
		break;
	}

	connect(ui.recordButton, &QPushButton::clicked, this, &MainWindow::toggleRecord);
}

void MainWindow::updateDeviceList()
{
	for (const QCameraInfo & cameraInfo : QCameraInfo::availableCameras())
	{
		ui.cameraListWidget->addItem(cameraInfo.description());
	}

	for (const QAudioDeviceInfo & audioDeviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
	{
		ui.micListWidget->addItem(audioDeviceInfo.deviceName());
	}
}

void MainWindow::toggleRecord()
{
	if (recorder.isRecording())
	{
		recorder.stop();
	}
	else
	{
		QString filename = QFileDialog::getSaveFileName(this, QString(), QString(), "MPEG4 files (*.mp4)");
		if (!filename.isEmpty())
		{
			recorder.start(selectedCamera, filename);
		}
	}
}