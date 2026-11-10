// Temperature / humidity screen with a small animated weather icon.
#ifndef BRAIN_INFOSCREEN_H
#define BRAIN_INFOSCREEN_H

#include "../core/Canvas.h"

namespace InfoScreen {

// `time` in seconds (drives the animation of the icon).
void draw(Canvas &c, float tempC, float humidity, float time);

}  // namespace InfoScreen

#endif
