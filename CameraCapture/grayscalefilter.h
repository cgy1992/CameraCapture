#pragma once

#include "imagefilter.h"


class GrayscaleFilter : public ImageFilter
{
public:
	void filter(QImage & image);
};

