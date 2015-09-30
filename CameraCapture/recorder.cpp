#include "recorder.h"

#include <QDateTime>
#include <QCameraImageCapture>

#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <Dbt.h>
#include <shlwapi.h>
#include <comdef.h>


HRESULT ConfigureEncoder(
	IMFSinkWriter *writer,
	DWORD *streamIndex
	)
{
	HRESULT hr = S_OK;

	IMFMediaType *outputMediaType = NULL;

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

	outputMediaType->Release();

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

	return hr;
}

Recorder::Recorder(QObject *parent)
	: QObject(parent)
{
	HRESULT hr;

	hr = MFStartup(MF_VERSION);
	if (SUCCEEDED(hr))
	{
		qDebug() << "MFStartup ok";
	}
	else
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

void Recorder::start(QCamera * camera, const QString & filename)
{
	if (recording)
	{
		qWarning() << "Recorder::start failed: already started";
		return;
	}

	IMFAttributes * writerAttr;

	HRESULT hr;
	hr = MFCreateAttributes(&writerAttr, 2);

	if (FAILED(hr))
	{
		qWarning() << "MFCreateAttributes writer failed";
	}

	writerAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);

	writer = nullptr;
	hr = MFCreateSinkWriterFromURL(
		filename.toStdWString().c_str(),
		NULL,
		writerAttr,
		&writer
		);
	DWORD streamIndex;
	if (SUCCEEDED(hr))
	{
		hr = ConfigureEncoder(writer, &streamIndex);
	}
	else
	{
		qWarning() << "writer create failed";
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->BeginWriting();
	}
	else
	{
		qWarning() << "BeginWriting failed";
	}

	QCameraImageCapture * imageCapture = new QCameraImageCapture(camera);
	imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

	connect(imageCapture, &QCameraImageCapture::readyForCaptureChanged, this, [this, imageCapture](bool ready){
		if (ready && writer)
		{
			imageCapture->capture();
		}
	}, Qt::QueuedConnection);

	connect(imageCapture, &QCameraImageCapture::imageCaptured, this, [streamIndex, this](int id, QImage image){
		if (!writer) return;

		IMFSample * sample;
		HRESULT hr = MFCreateSample(&sample);
		IMFMediaBuffer * buffer;

		if (SUCCEEDED(hr))
		{
			hr = MFCreateMemoryBuffer(image.byteCount(), &buffer);
		}
		else
		{
			qWarning() << "MFCreateMemoryBuffer failed";
		}

		BYTE * bufferData;

		if (SUCCEEDED(hr))
		{
			hr = buffer->Lock(&bufferData, NULL, NULL);
		}
		else
		{
			qWarning() << "buffer->Lock failed";
		}

		if (SUCCEEDED(hr))
		{
			memcpy(bufferData, image.bits(), image.byteCount());
		}

		if (SUCCEEDED(hr))
		{
			hr = buffer->Unlock();
		}
		else
		{
			qWarning() << "buffer->Unlock failed";
		}

		if (SUCCEEDED(hr))
		{
			hr = buffer->SetCurrentLength(image.byteCount());
		}
		else
		{
			qWarning() << "buffer->SetCurrentLength failed";
		}

		if (SUCCEEDED(hr))
		{
			hr = sample->AddBuffer(buffer);
		}
		else
		{
			qWarning() << "AddBuffer failed";
		}

		static qint64 startTimestamp = QDateTime::currentMSecsSinceEpoch();
		static qint64 prevTimestamp = QDateTime::currentMSecsSinceEpoch();

		if (SUCCEEDED(hr))
		{
			hr = sample->SetSampleTime((QDateTime::currentMSecsSinceEpoch() - startTimestamp) * 1000 * 10);
		}
		else
		{
			qWarning() << "SetSampleTime failed";
		}

		if (SUCCEEDED(hr))
		{
			hr = sample->SetSampleDuration((QDateTime::currentMSecsSinceEpoch() - prevTimestamp) * 1000 * 10);
			prevTimestamp = QDateTime::currentMSecsSinceEpoch();
		}
		else
		{
			qWarning() << "SetSampleTime failed";
		}

		if (SUCCEEDED(hr))
		{
			hr = writer->WriteSample(streamIndex, sample);
		}
		else
		{
			qWarning() << "WriteSample failed";
		}
	}, Qt::QueuedConnection);

	if (SUCCEEDED(hr))
	{
		imageCapture->capture();
		recording = true;
	}
}

void Recorder::stop()
{
	if (writer)
	{
		writer->Finalize();
		writer->Release();
		writer = nullptr;
	}
	recording = false;
}

bool Recorder::isRecording() const
{
	return recording;
}

Recorder::~Recorder()
{
	stop();
	MFShutdown();
}
