#pragma once

#include <QObject>
#include <QCamera>
#include <QCameraImageCapture>
#include <QAudioDeviceInfo>
#include <QAudioInput>

class IMFSinkWriter;

class Recorder : public QObject
{
	Q_OBJECT

public:
	Recorder(QObject *parent = 0);
	~Recorder();

public slots:
	bool start(QCamera * camera, QAudioDeviceInfo audioDeviceInfo, const QString & filename);
	void stop();
	bool isRecording() const;

private:
	void writeFrame(int streamIndex, const QImage & image);
	void writeAudio(int streamIndex, QByteArray sound);

private:
	IMFSinkWriter * writer = nullptr;
	QCameraImageCapture * imageCapture = nullptr;
	QAudioInput * audioInput = nullptr;
	bool recording = false;
	qint64 startTimestamp;
	qreal audioClock;
	qint64 videoClock;
	QByteArray audioBuffer;
};

