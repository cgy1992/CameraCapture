#include "cameracapture.h"

#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QCameraImageCapture>
#include <QDebug>
#include <QDebug>
#include <QDateTime>

HRESULT ConfigureEncoder(
	IMFSinkWriter *writer,
	DWORD *streamIndex
	)
{
	HRESULT hr = S_OK;

	IMFMediaType *mediaType = NULL;

	hr = MFCreateMediaType(&mediaType);

	if (SUCCEEDED(hr))
	{
		hr = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}

	if (SUCCEEDED(hr))
	{
		hr = mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	}

	if (SUCCEEDED(hr))
	{
		hr = mediaType->SetUINT32(MF_MT_AVG_BITRATE, 2500000);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(mediaType, MF_MT_FRAME_SIZE, 320, 240);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(mediaType, MF_MT_FRAME_RATE, 30, 1);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(mediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}

	if (SUCCEEDED(hr))
	{
		hr = mediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown);
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->AddStream(mediaType, streamIndex);
	}

	mediaType->Release();

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

	// Set the input media type.
	IMFMediaType    *pMediaTypeIn = NULL;
	if (SUCCEEDED(hr))
	{
		hr = MFCreateMediaType(&pMediaTypeIn);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, 320, 240);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, 30, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, pMediaTypeIn, NULL);
	}

	return hr;
}


CameraCapture::CameraCapture(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

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

	IMFAttributes * writerAttr;

	if (SUCCEEDED(hr))
	{
		hr = MFCreateAttributes(&writerAttr, 2);
	}
	else
	{
		qWarning() << "MFCreateAttributes writer failed";
	}

	writerAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);

	writer = nullptr;
	hr = MFCreateSinkWriterFromURL(
		L"D:/mfout.mp4",
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
		qDebug() << "writer create failed";
	}

	if (SUCCEEDED(hr))
	{
		hr = writer->BeginWriting();
	}
	else
	{
		qDebug() << "BeginWriting failed";
	}


	QCameraViewfinder * viewfinder = new QCameraViewfinder();
	setCentralWidget(viewfinder);

	QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
	qDebug() << "camera count: " << cameras.size();
	foreach(const QCameraInfo &cameraInfo, cameras)
	{
		setWindowTitle(cameraInfo.description());
		QCamera * camera = new QCamera(cameraInfo);
		camera->setCaptureMode(QCamera::CaptureStillImage);
		camera->setViewfinder(viewfinder);
		camera->start();

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

			static qint64 prevTimestamp = QDateTime::currentMSecsSinceEpoch();

			if (SUCCEEDED(hr))
			{
				hr = sample->SetSampleTime(QDateTime::currentMSecsSinceEpoch() * 1000 * 10 );
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
		imageCapture->capture();
		break;
	}
}

CameraCapture::~CameraCapture()
{
	writer->Finalize();
	writer->Release();
	writer = nullptr;
	MFShutdown();
}

