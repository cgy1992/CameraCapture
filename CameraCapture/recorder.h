#pragma once

#include <QObject>
#include <QCamera>

class IMFSinkWriter;

class Recorder : public QObject
{
	Q_OBJECT

public:
	Recorder(QObject *parent = 0);
	~Recorder();

public slots:
	void start(QCamera * camera, const QString & filename);
	void stop();
	bool isRecording() const;

private:
	IMFSinkWriter * writer = nullptr;
	bool recording = false;
};

