#pragma once

#include "imagefilter.h"

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
	bool start(const QString & filename);
	void stop();
	bool isRecording() const;

	void setCamera(QCamera * camera);
	QCamera * camera() const;

	void setAudioDevice(const QAudioDeviceInfo & audioDeviceInfo);
	QAudioDeviceInfo audioDevice() const;

	void setImageFilter(ImageFilter * imageFilter);
	ImageFilter * imageFilter();

private:
	Q_DECLARE_PRIVATE(Recorder)
	QScopedPointer<RecorderPrivate> d_ptr;
};

