#include "recorder.h"

#include <QDateTime>
#include <QAudioFormat>

#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <Dbt.h>
#include <shlwapi.h>
#include <comdef.h>


HRESULT setupVideoStream(IMFSinkWriter *writer, DWORD *streamIndex)
{
	HRESULT hr = S_OK;

	IMFMediaType * outputMediaType = nullptr;

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
		hr = MFSetAttributeRatio(outputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}

	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown);
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->AddStream(outputMediaType, streamIndex);
	}

	if (outputMediaType)
	{
		outputMediaType->Release();
	}

	// Set the input media type.
	IMFMediaType * inputMediaType = nullptr;
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

	if (inputMediaType)
	{
		inputMediaType->Release();
	}

	return hr;
}

HRESULT setupAudioStream(IMFSinkWriter *writer, DWORD *streamIndex)
{
	int channels = 1;
	int bitsPerSample = 16;
	int sampleRate = 44100;

	HRESULT hr = S_OK;
	IMFMediaType * outputMediaType = nullptr;

	hr = MFCreateMediaType(&outputMediaType);

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
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
	}
	if (SUCCEEDED(hr))
	{
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->AddStream(outputMediaType, streamIndex);
	}

	if (outputMediaType)
	{
		outputMediaType->Release();
	}

	// Set the input media type.
	IMFMediaType * inputMediaType = nullptr;
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
		hr = inputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
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

	if (inputMediaType)
	{
		inputMediaType->Release();
	}

	return hr;
}

Recorder::Recorder(QObject *parent)
	: QObject(parent)
{
	HRESULT hr;

	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		qDebug() << "MFStartup failed";
	}

	if (SUCCEEDED(hr))
	{
		// Register the color converter DSP for this process, in the video 
		// processor category. This will enable the sink writer to enumerate
		// the color converter when the sink writer attempts to match the
		// media types.

		hr = MFTRegisterLocalByCLSID(
			__uuidof(CColorConvertDMO),
			MFT_CATEGORY_VIDEO_PROCESSOR,
			L"",
			MFT_ENUM_FLAG_SYNCMFT,
			0,
			NULL,
			0,
			NULL
			);
	}
	else
	{
		qWarning() << "MFTRegisterLocalByCLSID failed";
	}
}

bool Recorder::start(QCamera * camera, QAudioDeviceInfo audioDeviceInfo, const QString & filename)
{
	if (recording)
	{
		qWarning() << "Recorder::start failed: already started";
		return false;
	}

	IMFAttributes * writerAttr;

	HRESULT hr;
	hr = MFCreateAttributes(&writerAttr, 2);

	if (FAILED(hr))
	{
		qWarning() << "MFCreateAttributes writer failed";
	}

	writerAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
	writerAttr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

	writer = nullptr;
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSinkWriterFromURL(filename.toStdWString().c_str(), NULL, writerAttr, &writer);
		if (FAILED(hr))
		{
			qWarning() << "MFCreateSinkWriterFromURL failed" << QString::number(hr, 16);
		}
	}
	
	DWORD videoStreamIndex;
	if (SUCCEEDED(hr))
	{
		hr = setupVideoStream(writer, &videoStreamIndex);
		if (FAILED(hr))
		{
			qWarning() << "setupVideoStream failed" << QString::number(hr, 16);
		}
	}

	DWORD audioStreamIndex;
	
	if (SUCCEEDED(hr))
	{
		if (!audioDeviceInfo.isNull())
		{
			QAudioFormat format;
			format.setSampleRate(44100);
			format.setSampleSize(16);
			format.setChannelCount(1);
			format.setSampleType(QAudioFormat::SignedInt);
			format.setCodec("audio/pcm");
			audioInput = new QAudioInput(audioDeviceInfo, format);
		}

		hr = setupAudioStream(writer, &audioStreamIndex);
		if (FAILED(hr))
		{
			qWarning() << "setupAudioStream failed" << QString::number(hr, 16);
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->BeginWriting();
		if (FAILED(hr))
		{
			qWarning() << "BeginWriting failed" << QString::number(hr, 16);
		}
	}

	if (SUCCEEDED(hr))
	{
		imageCapture = new QCameraImageCapture(camera);
		imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

		connect(imageCapture, &QCameraImageCapture::readyForCaptureChanged, this, [this](bool ready){
			if (ready && writer)
			{
				qint64 s = QDateTime::currentMSecsSinceEpoch();
				imageCapture->capture();
				qDebug() << "capture" << (QDateTime::currentMSecsSinceEpoch() - s) << "ms";
			}
		}, Qt::QueuedConnection);

		connect(imageCapture, &QCameraImageCapture::imageCaptured, this, [=](int id, QImage image){
			if (writer)
			{
				writeFrame(videoStreamIndex, image);
			}
		});
		recording = true;
		startTimestamp = QDateTime::currentMSecsSinceEpoch();

		qint64 s = QDateTime::currentMSecsSinceEpoch();
		imageCapture->capture();
		qDebug() << "first capture" << (QDateTime::currentMSecsSinceEpoch() - s) << "ms";

		videoClock = 0;

		if (!audioDeviceInfo.isNull())
		{
			audioClock = 0;
			QIODevice * audioDevice = audioInput->start();
			connect(audioDevice, &QIODevice::readyRead, this, [=](){
				audioBuffer += audioDevice->readAll();
				qDebug() << "ready read" << audioBuffer.size();
				while (audioBuffer.size() > 1024)
				{
					QByteArray data = audioBuffer.left(1024);
					audioBuffer = audioBuffer.mid(1024);
					writeAudio(audioStreamIndex, data);
				}
			});
		}
		return true;
	}
	else
	{
		return false;
	}
}

void Recorder::stop()
{
	if (writer)
	{
		writer->Finalize();
		writer->Release();
		writer = nullptr;

		delete imageCapture;
		imageCapture = nullptr;

		if (audioInput)
		{
			delete audioInput;
			audioInput = nullptr;
		}
	}
	recording = false;
}

bool Recorder::isRecording() const
{
	return recording;
}

void Recorder::writeFrame(int streamIndex, const QImage & image)
{
	IMFSample * sample;
	HRESULT hr = MFCreateSample(&sample);
	IMFMediaBuffer * buffer;

	if (SUCCEEDED(hr))
	{
		hr = MFCreateMemoryBuffer(image.byteCount(), &buffer);
		if (FAILED(hr))
		{
			qWarning() << "MFCreateMemoryBuffer failed";
		}
	}

	BYTE * bufferData;

	if (SUCCEEDED(hr))
	{
		hr = buffer->Lock(&bufferData, NULL, NULL);
		if (FAILED(hr))
		{
			qWarning() << "buffer->Lock failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		memcpy(bufferData, image.bits(), image.byteCount());
	}

	if (SUCCEEDED(hr))
	{
		hr = buffer->Unlock();
		if (FAILED(hr))
		{
			qWarning() << "buffer->Unlock failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = buffer->SetCurrentLength(image.byteCount());
		if (FAILED(hr))
		{
			qWarning() << "buffer->SetCurrentLength failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = sample->AddBuffer(buffer);
		if (FAILED(hr))
		{
			qWarning() << "AddBuffer failed";
		}
	}

	static qint64 duration = 1000 * 1000 * 10 / 30;

	if (SUCCEEDED(hr))
	{
		hr = sample->SetSampleTime(videoClock);
		videoClock += duration;
		if (FAILED(hr))
		{
			qWarning() << "SetSampleTime failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = sample->SetSampleDuration(duration);
		if (FAILED(hr))
		{
			qWarning() << "SetSampleTime failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		qint64 s = QDateTime::currentMSecsSinceEpoch();
		hr = writer->WriteSample(streamIndex, sample);
		qDebug() << "WriteSample" << (QDateTime::currentMSecsSinceEpoch() - s) << "ms";
		if (FAILED(hr))
		{
			qWarning() << "WriteSample failed";
		}
	}
}

void Recorder::writeAudio(int streamIndex, QByteArray sound)
{
	IMFSample * sample;
	HRESULT hr = MFCreateSample(&sample);
	IMFMediaBuffer * buffer;

	if (SUCCEEDED(hr))
	{
		hr = MFCreateMemoryBuffer(sound.size(), &buffer);
		if (FAILED(hr))
		{
			qWarning() << "MFCreateMemoryBuffer failed";
		}
	}

	BYTE * bufferData;

	if (SUCCEEDED(hr))
	{
		hr = buffer->Lock(&bufferData, NULL, NULL);
		if (FAILED(hr))
		{
			qWarning() << "buffer->Lock failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		memcpy(bufferData, sound, sound.size());
	}

	if (SUCCEEDED(hr))
	{
		hr = buffer->Unlock();
		if (FAILED(hr))
		{
			qWarning() << "buffer->Unlock failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = buffer->SetCurrentLength(sound.size());
		if (FAILED(hr))
		{
			qWarning() << "buffer->SetCurrentLength failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = sample->AddBuffer(buffer);
		if (FAILED(hr))
		{
			qWarning() << "AddBuffer failed";
		}
	}

	static qreal duration = 1024 * 1000 * 10 / 44.100;

	if (SUCCEEDED(hr))
	{
		hr = sample->SetSampleTime(audioClock);
		audioClock += duration;
		if (FAILED(hr))
		{
			qWarning() << "SetSampleTime failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = sample->SetSampleDuration(duration);
		if (FAILED(hr))
		{
			qWarning() << "SetSampleTime failed";
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->WriteSample(streamIndex, sample);
		if (FAILED(hr))
		{
			qWarning() << "WriteSample failed";
		}
	}
}

Recorder::~Recorder()
{
	stop();
	MFShutdown();
}