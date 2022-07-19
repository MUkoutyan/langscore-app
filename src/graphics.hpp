#pragma once
#include <QImage>

namespace graphics {

static void ReverceHSVValue(QImage& image)
{
    for (int i = 0; i < image.width(); i++)
    {
        for (int j = 0; j < image.height(); j++)
        {
            QColor color = image.pixelColor(i, j);

            color.setHsv(color.hue(), color.saturation(), 255-color.value(), color.alpha());
            image.setPixelColor(i, j, color);
        }
    }
}

}
