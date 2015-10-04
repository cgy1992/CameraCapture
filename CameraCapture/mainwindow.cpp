#include "mainwindow.h"

#include <QCamera>
#include <QAudio>
#include <QFileDialog>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	grayscaleFilter(new GrayscaleFilter)
{
	ui.setupUi(this);

	cameraInfoList = QCameraInfo::availableCameras();
	if (cameraInfoList.isEmpty())
	{
		QMessageBox::information(this, tr("Camera not found"), tr("Camera not found.\nPlease, install camera on your computer and then restart program."));
		qApp->quit();
		return;
	}

	selectCamera(cameraInfoList.first());
	for (const QCameraInfo & cameraInfo : cameraInfoList)
	{
		ui.cameraListWidget->addItem(cameraInfo.description());
	}
	ui.cameraListWidget->setCurrentRow(0);

	micList = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
	for (const QAudioDeviceInfo & audioDeviceInfo : micList)
	{
		ui.micListWidget->addItem(audioDeviceInfo.deviceName());
	}
	if (!micList.isEmpty())
	{
		selectAudioDevice(micList.first());
		ui.micListWidget->setCurrentRow(0);
	}

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

	connect(ui.recordButton, &QPushButton::clicked, this, &MainWindow::toggleRecord);

	connect(ui.cameraListWidget, &QListWidget::currentRowChanged, this, [this](int currentRow){
		selectCamera(cameraInfoList.at(currentRow));
	});

	connect(ui.micListWidget, &QListWidget::currentRowChanged, this, [this](int currentRow){
		selectAudioDevice(micList.at(currentRow));
	});

	uiUpdateTimer.setInterval(1000);
	uiUpdateTimer.setSingleShot(false);
	connect(&uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updateRecordButton);
}

void MainWindow::toggleRecord()
{
	if (recorder.isRecording())
	{
		recorder.stop();
		uiUpdateTimer.stop();
	}
	else
	{
		QString filename = QFileDialog::getSaveFileName(this, QString(), QString(), "MPEG4 files (*.mp4)");
		if (filename.isEmpty())
		{
			ui.recordButton->setChecked(false);
			return;
		}
		
		if (!recorder.start(filename))
		{
			ui.recordButton->setChecked(false);
			QMessageBox::warning(this, tr("Record failed"), tr("Record failed.\nPlease, check program output log for more information"));
		}
		else
		{
			recordTimer.start();
			uiUpdateTimer.start();
		}
	}

	updateRecordButton();
}

void MainWindow::selectCamera(const QCameraInfo & cameraInfo)
{
	selectedCamera = std::make_unique<QCamera>(cameraInfo);
	selectedCamera->setCaptureMode(QCamera::CaptureStillImage);
	selectedCamera->setViewfinder(ui.viewfinder);
	selectedCamera->start();

	recorder.setCamera(selectedCamera.get());
}

void MainWindow::selectAudioDevice(const QAudioDeviceInfo & audioDeviceInfo)
{
	recorder.setAudioDevice(audioDeviceInfo);
}

void MainWindow::updateRecordButton()
{
	if (recorder.isRecording())
	{
		int min = recordTimer.elapsed() / 1000 / 60;
		int sec = recordTimer.elapsed() / 1000 % 60;
		ui.recordButton->setText(tr("STOP RECORD (%1:%2)").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));
	}
	else
	{
		ui.recordButton->setText(tr("RECORD"));
	}
}