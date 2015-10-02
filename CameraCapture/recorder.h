#pragma once

#include <QObject>
#include <QCamera>
#include <QAudioDeviceInfo>

#include <QAudioInput>
#include <QCameraImageCapture>
#include <QByteArray>
#include <atlbase.h>
#include <memory>
class IMFSinkWriter;

class Recorder : public QObject
{
	Q_OBJECT

public:
	Recorder(QObject *parent = 0);
	~Recorder();

public slots:
	bool start(QCamera * camera, const QAudioDeviceInfo & audioDeviceInfo, const QString & filename);
	void stop();
	bool isRecording() const;

private:
	void writeVideoFrame(int streamIndex, const QImage & image);
	void writeAudioFrame(int streamIndex, const QByteArray & sound);

private:
	CComPtr<IMFSinkWriter> writer;
	std::unique_ptr<QCameraImageCapture> imageCapture;
	std::unique_ptr<QAudioInput> audioInput;
	bool recording = false;
	qreal audioClock;
	qint64 videoClock;
	QByteArray audioBuffer;
};

