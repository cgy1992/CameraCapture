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

class RecorderPrivate
{
	Q_DECLARE_PUBLIC(Recorder)
	Recorder * q_ptr;

	CComPtr<IMFSinkWriter> writer;
	std::unique_ptr<QCameraImageCapture> imageCapture;
	std::unique_ptr<QAudioInput> audioInput;
	bool recording = false;
	qreal audioClock = 0;
	qreal videoClock = 0;
	QByteArray audioBuffer;
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
		hr = outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 2500000);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(outputMediaType, MF_MT_FRAME_SIZE, 320, 240);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(outputMediaType, MF_MT_FRAME_RATE, 30, 1);
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
		hr = MFSetAttributeSize(inputMediaType, MF_MT_FRAME_SIZE, 320, 240);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(inputMediaType, MF_MT_FRAME_RATE, 30, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(inputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, inputMediaType, NULL);
	}

	return hr;
}

HRESULT setupAudioStream(IMFSinkWriter *writer, UINT32 channelCount, UINT32 bitsPerSample, UINT32 sampleRate, DWORD *streamIndex)
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
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 20000);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channelCount);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
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
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channelCount);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
	}
	if (SUCCEEDED(hr))
	{
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, inputMediaType, NULL);
	}

	return hr;
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

bool Recorder::start(QCamera * camera, const QAudioDeviceInfo & audioDeviceInfo, const QString & filename)
{
	Q_D(Recorder);
	if (d->recording)
	{
		qWarning() << "Recorder::start failed: already started";
		return false;
	}

	HRESULT hr;

	CComPtr<IMFAttributes> writerAttr;
	hr = MFCreateAttributes(&writerAttr, 2);
	if (FAILED(hr))
	{
		qWarning() << "MFCreateAttributes writer failed" << hr;
		return false;
	}

	hr = writerAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
	if (FAILED(hr))
	{
		qWarning() << "writerAttr->SetGUID MF_TRANSCODE_CONTAINERTYPE failed" << hr;
		return false;
	}

	hr = writerAttr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
	if (FAILED(hr))
	{
		qWarning() << "writerAttr->SetUINT32 MF_SINK_WRITER_DISABLE_THROTTLING failed" << hr;
		return false;
	}

	CComPtr<IMFSinkWriter> writer;
	hr = MFCreateSinkWriterFromURL(filename.toStdWString().c_str(), NULL, writerAttr, &writer);
	if (FAILED(hr))
	{
		qWarning() << "MFCreateSinkWriterFromURL failed" << hr;
		return false;
	}
	
	DWORD videoStreamIndex;
	hr = setupVideoStream(writer, &videoStreamIndex);
	if (FAILED(hr))
	{
		qWarning() << "setupVideoStream failed" << hr;
		return false;
	}

	DWORD audioStreamIndex;
	int audioChannelCount = 2;
	
	if (!audioDeviceInfo.isNull())
	{
		hr = setupAudioStream(writer, audioChannelCount, 16, 44100, &audioStreamIndex);
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

	d->imageCapture = std::make_unique<QCameraImageCapture>(camera);
	d->imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

	connect(d->imageCapture.get(), &QCameraImageCapture::readyForCaptureChanged, this, [d](bool ready){
		if (ready && d->imageCapture)
		{
			d->imageCapture->capture();
		}
	}, Qt::QueuedConnection);

	connect(d->imageCapture.get(), &QCameraImageCapture::imageCaptured, this, [=](int id, QImage image){
		if (d->writer)
		{
			if (d->imageFilter)
			{
				d->imageFilter->filter(image);
			}
			writeVideoFrame(videoStreamIndex, image);
		}
	});

	d->writer = writer;
	d->recording = true;
	d->videoClock = 0;
	d->imageCapture->capture();

	if (!audioDeviceInfo.isNull())
	{
		QAudioFormat format;
		format.setSampleRate(44100);
		format.setSampleSize(16);
		format.setChannelCount(audioChannelCount);
		format.setSampleType(QAudioFormat::SignedInt);
		format.setCodec("audio/pcm");
		d->audioInput = std::make_unique<QAudioInput>(audioDeviceInfo, format);
		d->audioClock = 0;
		QIODevice * audioDevice = d->audioInput->start();
		connect(audioDevice, &QIODevice::readyRead, this, [=](){
			d->audioBuffer += audioDevice->readAll();
			int frameSize = 1024 * audioChannelCount;
			while (d->audioBuffer.size() > frameSize)
			{
				QByteArray data = d->audioBuffer.left(frameSize);
				d->audioBuffer.remove(0, frameSize);
				writeAudioFrame(audioStreamIndex, data);
			}
		});
	}

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

	d->audioBuffer.clear();
}

bool Recorder::isRecording() const
{
	return d_func()->recording;
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

	static qreal duration = 1000.0 * 1000 * 10 / 30;

	hr = sample->SetSampleTime(d->videoClock);
	d->videoClock += duration;
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = sample->SetSampleDuration(duration);
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

	// 512 = 1024 / 2 bytes per sample
	static qreal duration = 512.0 * 1000 * 1000 * 10 / 44100;

	hr = sample->SetSampleTime(d->audioClock);
	d->audioClock += duration;
	if (FAILED(hr))
	{
		qWarning() << "SetSampleTime failed" << hr;
		return false;
	}

	hr = sample->SetSampleDuration(duration);
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
