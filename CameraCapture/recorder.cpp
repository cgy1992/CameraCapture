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
};

HRESULT setupVideoStream(IMFSinkWriter *writer, DWORD *streamIndex)
{
	HRESULT hr = S_OK;

	CComPtr<IMFMediaType> outputMediaType;

	hr = MFCreateMediaType(&outputMediaType);

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	}

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, VideoBitrate);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(outputMediaType, MF_MT_FRAME_SIZE, VideoResolution.width(), VideoResolution.height());
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(outputMediaType, MF_MT_FRAME_RATE, FramesPerSecond, 1);
	}

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown);
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->AddStream(outputMediaType, streamIndex);
	}

	CComPtr<IMFMediaType> inputMediaType;
	if (SUCCEEDED(hr))
	{
		hr = MFCreateMediaType(&inputMediaType);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(inputMediaType, MF_MT_FRAME_SIZE, VideoResolution.width(), VideoResolution.height());
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, inputMediaType, NULL);
	}

	return hr;
}

HRESULT setupAudioStream(IMFSinkWriter *writer, const QAudioFormat & audioFormat, DWORD *streamIndex)
{
	CComPtr<IMFMediaType> outputMediaType;

	HRESULT hr = MFCreateMediaType(&outputMediaType);

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AudioBitrate);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, audioFormat.sampleSize());
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, audioFormat.channelCount());
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, audioFormat.sampleRate());
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->AddStream(outputMediaType, streamIndex);
	}

	CComPtr<IMFMediaType> inputMediaType;
	if (SUCCEEDED(hr))
	{
		hr = MFCreateMediaType(&inputMediaType);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, audioFormat.channelCount());
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, audioFormat.sampleRate());
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, audioFormat.sampleSize());
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, inputMediaType, NULL);
	}

	return hr;
}

HRESULT createWriter(QString url, IMFSinkWriter ** writer)
{
	HRESULT hr = S_OK;

	CComPtr<IMFAttributes> writerAttr;
	hr = MFCreateAttributes(&writerAttr, 2);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = writerAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = writerAttr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = MFCreateSinkWriterFromURL(url.toStdWString().c_str(), NULL, writerAttr, writer);
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}

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
	
	hr = setupVideoStream(writer, &d->videoStreamIndex);
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
		hr = setupAudioStream(writer, d->audioFormat, &d->audioStreamIndex);
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

	// frames durations in 100-nanosecond units
	// 1024 - AAC frame sample count
	d->audioFrameDuration = 1024.0 * 1000 * 1000 * 10 / audioFormat.channelCount() / audioFormat.sampleRate();
	d->videoFrameDuration = 1000.0 * 1000 * 10 / FramesPerSecond;

	d->writer = writer;
	d->recording = true;
	d->videoClock = 0;
	d->audioClock = 0;

	startImageCapture();
	startAudioCapture();

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

void Recorder::startImageCapture()
{
	Q_D(Recorder);

	d->imageCapture = std::make_unique<QCameraImageCapture>(d->camera);
	d->imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
	d->imageCapture->setBufferFormat(QVideoFrame::Format_RGB32);
	d->imageCapture->capture();

	connect(d->imageCapture.get(), &QCameraImageCapture::readyForCaptureChanged, d->imageCapture.get(), [d](bool ready){
		if (ready && d->imageCapture)
		{
			d->imageCapture->capture();
		}
	}, Qt::QueuedConnection);

	connect(d->imageCapture.get(), &QCameraImageCapture::imageCaptured, d->imageCapture.get(), [d, this](int id, QImage image){
		if (d->writer)
		{
			image = image.scaled(VideoResolution);
			if (d->imageFilter)
			{
				d->imageFilter->filter(image);
			}
			writeVideoFrame(d->videoStreamIndex, image);
		}
	});
}

void Recorder::startAudioCapture()
{
	Q_D(Recorder);

	if (d->audioDeviceInfo.isNull())
	{
		d->audioInput = nullptr;
		return;
	}
	
	d->audioInput = std::make_unique<QAudioInput>(d->audioDeviceInfo, d->audioFormat);
	QIODevice * audioDevice = d->audioInput->start();
	d->audioBuffer.clear();
	connect(audioDevice, &QIODevice::readyRead, audioDevice, [=](){
		d->audioBuffer += audioDevice->readAll();
		int frameSize = 1024 * d->audioFormat.channelCount();
		while (d->audioBuffer.size() > frameSize)
		{
			QByteArray data = d->audioBuffer.left(frameSize);
			d->audioBuffer.remove(0, frameSize);
			writeAudioFrame(d->audioStreamIndex, data);
		}
	});
}

bool Recorder::writeVideoFrame(int streamIndex, const QImage & image)
{
	Q_D(Recorder);

	CComPtr<IMFSample> sample;
	CComPtr<IMFMediaBuffer> buffer;

	HRESULT hr = MFCreateSample(&sample);

	hr = MFCreateMemoryBuffer(image.byteCount(), &buffer);
	if (FAILED(hr))
	{
		qWarning() << "MFCreateMemoryBuffer failed" << hr;
		return false;
	}

	BYTE * bufferData;

	hr = buffer->Lock(&bufferData, NULL, NULL);
	if (FAILED(hr))
	{
		qWarning() << "buffer->Lock failed" << hr;
		return false;
	}

	memcpy(bufferData, image.bits(), image.byteCount());

	hr = buffer->Unlock();
	if (FAILED(hr))
	{
		qWarning() << "buffer->Unlock failed" << hr;
		return false;
	}

	hr = buffer->SetCurrentLength(image.byteCount());
	if (FAILED(hr))
	{
		qWarning() << "buffer->SetCurrentLength failed" << hr;
		return false;
	}

	hr = sample->AddBuffer(buffer);
	if (FAILED(hr))
	{
		qWarning() << "AddBuffer failed" << hr;
		return false;
	}

	hr = sample->SetSampleTime(d->videoClock);
	d->videoClock += d->videoFrameDuration;
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = sample->SetSampleDuration(d->videoFrameDuration);
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = d->writer->WriteSample(streamIndex, sample);
	if (FAILED(hr))
	{
		qWarning() << "WriteSample failed" << hr;
		return false;
	}

	return true;
}

bool Recorder::writeAudioFrame(int streamIndex, const QByteArray & sound)
{
	Q_D(Recorder);
	CComPtr<IMFSample> sample;
	CComPtr<IMFMediaBuffer> buffer;

	HRESULT hr = MFCreateSample(&sample);

	hr = MFCreateMemoryBuffer(sound.size(), &buffer);
	if (FAILED(hr))
	{
		qWarning() << "MFCreateMemoryBuffer failed" << hr;
		return false;
	}

	BYTE * bufferData;

	hr = buffer->Lock(&bufferData, NULL, NULL);
	if (FAILED(hr))
	{
		qWarning() << "buffer->Lock failed" << hr;
		return false;
	}

	memcpy(bufferData, sound, sound.size());

	hr = buffer->Unlock();
	if (FAILED(hr))
	{
		qWarning() << "buffer->Unlock failed" << hr;
		return false;
	}

	hr = buffer->SetCurrentLength(sound.size());
	if (FAILED(hr))
	{
		qWarning() << "buffer->SetCurrentLength failed" << hr;
		return false;
	}

	hr = sample->AddBuffer(buffer);
	if (FAILED(hr))
	{
		qWarning() << "AddBuffer failed" << hr;
		return false;
	}

	hr = sample->SetSampleTime(d->audioClock);
	d->audioClock += d->audioFrameDuration;
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = sample->SetSampleDuration(d->audioFrameDuration);
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = d->writer->WriteSample(streamIndex, sample);
	if (FAILED(hr))
	{
		qWarning() << "WriteSample failed" << hr;
		return false;
	}

	return true;
}

void Recorder::setCamera(QCamera * camera)
{
	Q_D(Recorder);
	d->camera = camera;
	if (d->recording)
	{
		startImageCapture();
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
		startAudioCapture();
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

ImageFilter * Recorder::imageFilter()
{
	Q_D(Recorder);
	return d->imageFilter;
}
