#include "recorder.h"

#include <memory>

#include <QDateTime>
#include <QAudioFormat>
#include <QAudioInput>
#include <QCameraImageCapture>
#include <QByteArray>

#include <atlbase.h>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include "wmfutils.h"

namespace
{
	// \todo constants should be parameters of Recorder class

	static const QSize VideoResolution(320, 240);
	static const int VideoBitrate = 2500000;
	static const int FramesPerSecond = 30;

	static const int AudioBitrate = 20000;
	static const int AudioSampleRate = 44100;
	static const int AudioSampleSize = 16;
	static const int AudioChannelCount = 2;

	static const qint64 SecondInUnits = 1000 * 1000 * 10;
}

class RecorderPrivate
{
	Q_DECLARE_PUBLIC(Recorder)
	Recorder * q_ptr;

	CComPtr<IMFSinkWriter> writer;
	QCamera * camera = nullptr;
	QAudioDeviceInfo audioDeviceInfo;
	bool recording = false;
	std::unique_ptr<QCameraImageCapture> imageCapture;
	std::unique_ptr<QAudioInput> audioInput;
	DWORD videoStreamIndex = -1;
	DWORD audioStreamIndex = -1;
	qreal audioClock = 0;
	qreal videoClock = 0;
	QByteArray audioBuffer;
	QAudioFormat audioFormat;
	qreal audioFrameDuration;
	qreal videoFrameDuration;
	ImageFilter * imageFilter = nullptr;

	void startImageCapture()
	{
		imageCapture = std::make_unique<QCameraImageCapture>(camera);
		imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
		imageCapture->setBufferFormat(QVideoFrame::Format_RGB32);
		imageCapture->capture();

		QObject::connect(imageCapture.get(), &QCameraImageCapture::readyForCaptureChanged, imageCapture.get(), [this](bool ready){
			if (ready && imageCapture)
			{
				imageCapture->capture();
			}
		}, Qt::QueuedConnection);

		QObject::connect(imageCapture.get(), &QCameraImageCapture::imageCaptured, imageCapture.get(), [this](int id, QImage image){
			if (writer)
			{
				image = image.scaled(VideoResolution);
				if (imageFilter)
				{
					imageFilter->filter(image);
				}
				writeVideoFrame(videoStreamIndex, image);
			}
		});
	}

	void startAudioCapture()
	{
		if (audioDeviceInfo.isNull())
		{
			audioInput = nullptr;
			return;
		}

		audioInput = std::make_unique<QAudioInput>(audioDeviceInfo, audioFormat);
		QIODevice * audioDevice = audioInput->start();
		audioBuffer.clear();
		QObject::connect(audioDevice, &QIODevice::readyRead, audioDevice, [=](){
			audioBuffer += audioDevice->readAll();
			int frameSize = 1024 * audioFormat.channelCount();
			while (audioBuffer.size() > frameSize)
			{
				QByteArray data = audioBuffer.left(frameSize);
				audioBuffer.remove(0, frameSize);
				writeAudioFrame(audioStreamIndex, data);
			}
		});
	}

	bool writeVideoFrame(int streamIndex, const QImage & image)
	{
		CComPtr<IMFSample> sample;
		HRESULT hr = createSample(image.bits(), image.byteCount(), videoClock, videoFrameDuration, sample);
		if (FAILED(hr))
		{
			qWarning() << "createSample failed" << hr;
			return false;
		}

		videoClock += videoFrameDuration;

		hr = writer->WriteSample(streamIndex, sample);
		if (FAILED(hr))
		{
			qWarning() << "WriteSample failed" << hr;
			return false;
		}

		return true;
	}

	bool writeAudioFrame(int streamIndex, const QByteArray & sound)
	{
		CComPtr<IMFSample> sample;
		HRESULT hr = createSample(sound, sound.size(), audioClock, videoFrameDuration, sample);
		if (FAILED(hr))
		{
			qWarning() << "createSample failed" << hr;
			return false;
		}

		audioClock += audioFrameDuration;

		hr = writer->WriteSample(streamIndex, sample);
		if (FAILED(hr))
		{
			qWarning() << "WriteSample failed" << hr;
			return false;
		}

		return true;
	}
};

Recorder::Recorder(QObject *parent)
	: QObject(parent),
	d_ptr(new RecorderPrivate)
{
	d_ptr->q_ptr = this;

	HRESULT hr;

	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		qWarning() << "MFStartup failed" << hr;
		return;
	}

	// Register the color converter DSP for this process, in the video 
	// processor category. This will enable the sink writer to enumerate
	// the color converter when the sink writer attempts to match the
	// media types.
	hr = MFTRegisterLocalByCLSID(__uuidof(CColorConvertDMO), MFT_CATEGORY_VIDEO_PROCESSOR, L"", MFT_ENUM_FLAG_SYNCMFT, 0, NULL, 0, NULL);
	if (FAILED(hr))
	{
		qWarning() << "MFTRegisterLocalByCLSID failed" << hr;
		return;
	}
}

Recorder::~Recorder()
{
	stop();
	MFShutdown();
}

bool Recorder::start(const QString & filename)
{
	Q_D(Recorder);
	if (d->recording)
	{
		qWarning() << "Recorder::start failed: already started";
		return false;
	}

	if (!d->camera)
	{
		qWarning() << "Please, setup camera with Recorder::setCamera before calling Recorder::start";
		return false;
	}

	HRESULT hr;

	CComPtr<IMFSinkWriter> writer;
	hr = createWriter(filename, &writer);
	if (FAILED(hr))
	{
		qWarning() << "createWriter failed" << hr;
		return false;
	}
	
	hr = addVideoStream(VideoBitrate, VideoResolution, FramesPerSecond, writer, &d->videoStreamIndex);
	if (FAILED(hr))
	{
		qWarning() << "setupVideoStream failed" << hr;
		return false;
	}

	QAudioFormat audioFormat;
	audioFormat.setSampleRate(AudioSampleRate);
	audioFormat.setSampleSize(AudioSampleSize);
	audioFormat.setChannelCount(AudioChannelCount);
	audioFormat.setSampleType(QAudioFormat::SignedInt);
	audioFormat.setCodec("audio/pcm");
	d->audioFormat = d->audioDeviceInfo.nearestFormat(audioFormat);
	
	if (!d->audioDeviceInfo.isNull())
	{
		hr = addAudioStream(AudioBitrate, d->audioFormat, writer, &d->audioStreamIndex);
		if (FAILED(hr))
		{
			qWarning() << "setupAudioStream failed" << hr;
			return false;
		}
	}

	hr = writer->BeginWriting();
	if (FAILED(hr))
	{
		qWarning() << "BeginWriting failed" << hr;
		return false;
	}

	// 1024 - AAC frame sample count
	d->audioFrameDuration = 1024.0 * SecondInUnits / audioFormat.channelCount() / audioFormat.sampleRate();
	d->videoFrameDuration = qreal(SecondInUnits) / FramesPerSecond;

	d->writer = writer;
	d->recording = true;
	d->videoClock = 0;
	d->audioClock = 0;

	d->startImageCapture();
	d->startAudioCapture();

	return true;
}

void Recorder::stop()
{
	Q_D(Recorder);

	if (d->writer)
	{
		d->writer->Finalize();
		d->writer.Release();
	}

	d->imageCapture.reset();
	d->audioInput.reset();

	d->recording = false;
}

bool Recorder::isRecording() const
{
	return d_func()->recording;
}

void Recorder::setCamera(QCamera * camera)
{
	Q_D(Recorder);
	d->camera = camera;
	if (d->recording)
	{
		d->startImageCapture();
	}
}

QCamera * Recorder::camera() const
{
	return d_func()->camera;
}

void Recorder::setAudioDevice(const QAudioDeviceInfo & audioDeviceInfo)
{
	Q_D(Recorder);
	d->audioDeviceInfo = audioDeviceInfo;
	if (d->recording)
	{
		d->startAudioCapture();
	}
}

QAudioDeviceInfo Recorder::audioDevice() const
{
	return d_func()->audioDeviceInfo;
}

void Recorder::setImageFilter(ImageFilter * imageFilter)
{
	Q_D(Recorder);
	d->imageFilter = imageFilter;
}

ImageFilter * Recorder::imageFilter() const
{
	return d_func()->imageFilter;
}
