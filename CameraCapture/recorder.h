#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QCamera>
#include <QAudioDeviceInfo>

class RecorderPrivate;
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
	Q_DECLARE_PRIVATE(Recorder)
	QScopedPointer<RecorderPrivate> d_ptr;
};

