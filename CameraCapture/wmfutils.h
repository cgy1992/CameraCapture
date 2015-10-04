#pragma once


HRESULT addVideoStream(UINT32 bitrate, QSize resolution, UINT32 fps, IMFSinkWriter *writer, DWORD *streamIndex)
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
		hr = outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(outputMediaType, MF_MT_FRAME_SIZE, resolution.width(), resolution.height());
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(outputMediaType, MF_MT_FRAME_RATE, fps, 1);
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
		hr = MFSetAttributeSize(inputMediaType, MF_MT_FRAME_SIZE, resolution.width(), resolution.height());
	}
	if (SUCCEEDED(hr))
	{
		hr = writer->SetInputMediaType(*streamIndex, inputMediaType, NULL);
	}

	return hr;
}

HRESULT addAudioStream(UINT32 bitrate, const QAudioFormat & audioFormat, IMFSinkWriter *writer, DWORD *streamIndex)
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
		hr = outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bitrate);
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

HRESULT createSample(const void * data, DWORD size, LONGLONG sampleTime, LONGLONG sampleDuration, CComPtr<IMFSample> & sample)
{
	HRESULT hr = MFCreateSample(&sample);
	if (FAILED(hr))
	{
		return hr;
	}

	CComPtr<IMFMediaBuffer> buffer;
	hr = MFCreateMemoryBuffer(size, &buffer);
	if (FAILED(hr))
	{
		return hr;
	}

	BYTE * bufferData;

	hr = buffer->Lock(&bufferData, NULL, NULL);
	if (FAILED(hr))
	{
		return hr;
	}

	memcpy(bufferData, data, size);

	hr = buffer->Unlock();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = buffer->SetCurrentLength(size);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = sample->AddBuffer(buffer);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = sample->SetSampleTime(sampleTime);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = sample->SetSampleDuration(sampleDuration);
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}