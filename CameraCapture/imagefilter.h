#pragma once

#include <QImage>


class ImageFilter
{
public:
	virtual ~ImageFilter() {};

	virtual void filter(QImage & image) = 0;
};