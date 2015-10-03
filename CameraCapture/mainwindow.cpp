#include "mainwindow.h"

#include <QCamera>
#include <QCameraInfo>
#include <QFileDialog>
#include <QAudioDeviceInfo>
#include <QAudio>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	grayscaleFilter(new GrayscaleFilter)
{
	ui.setupUi(this);
	updateDeviceList();

	if (ui.grayscaleCheckBox->isChecked())
	{
		recorder.setImageFilter(grayscaleFilter.get());
	}

	connect(ui.grayscaleCheckBox, &QCheckBox::clicked, this, [this](bool checked){
		if (checked)
		{
			recorder.setImageFilter(grayscaleFilter.get());
		}
		else
		{
			recorder.setImageFilter(nullptr);
		}
	});

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
		if (filename.isEmpty())
		{
			ui.recordButton->setChecked(false);
			return;
		}

		QList<QAudioDeviceInfo> availableDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
		QAudioDeviceInfo audioDeviceInfo;

		int selectedMic = ui.micListWidget->currentRow();
		if (selectedMic < ui.micListWidget->count() && selectedMic >= 0)
		{
			audioDeviceInfo = availableDevices[selectedMic];
		}
		
		if (!recorder.start(selectedCamera, audioDeviceInfo, filename))
		{
			ui.recordButton->setChecked(false);
		}
	}

	ui.cameraListWidget->setDisabled(recorder.isRecording());
	ui.micListWidget->setDisabled(recorder.isRecording());
}