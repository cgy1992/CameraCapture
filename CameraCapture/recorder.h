#pragma once

#include "imagefilter.h"

#include <QObject>
#include <QScopedPointer>
#include <QCamera>
#include <QAudioDeviceInfo>


class RecorderPrivate;
/**
@brief The Recorder class is used for the recording of mpeg4 multimedia stream (H264/AAC).
@todo Media options (fps, bitrate, etc) should be parameters of Recorder class.
@todo Sync audio and video streams.
@todo Make recording in separate thread.
@todo Remove setCamera on-the-fly lag.
*/
class Recorder : public QObject
{
	Q_OBJECT

public:
	Recorder(QObject *parent = 0);
	~Recorder();

public slots:
	/**
	@return true if record started, false if not
	*/
	bool start(const QString & filename);
	void stop();
	bool isRecording() const;

	void setCamera(QCamera * camera);
	QCamera * camera() const;

	void setAudioDevice(const QAudioDeviceInfo & audioDeviceInfo);
	QAudioDeviceInfo audioDevice() const;

	void setImageFilter(ImageFilter * imageFilter);
	ImageFilter * imageFilter() const;

private:
	Q_DECLARE_PRIVATE(Recorder)
	QScopedPointer<RecorderPrivate> d_ptr;
};

