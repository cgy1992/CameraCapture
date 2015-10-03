#include "grayscalefilter.h"


void GrayscaleFilter::filter(QImage & image)
{
	for (int i = 0; i < image.height(); i++) {
		QRgb *pixel = reinterpret_cast<QRgb*>(image.scanLine(i));
		QRgb *end = pixel + image.width();
		for (; pixel != end; pixel++) {
			int gray = qGray(*pixel);
			*pixel = qRgb(gray, gray, gray);
		}
	}
}
