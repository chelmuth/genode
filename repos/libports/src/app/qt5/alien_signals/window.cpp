/*
 * \brief   Alien signals in Qt5
 * \author  Christian Helmuth
 * \date    2015-01-14
 */

/* Qt5 includes */
#include <QString>

/* Genode includes */
#include <base/printf.h>

/* local includes */
#include "window.h"


void Window::tick()
{
	PLOG("tock");

	_ticks->setText(QString::number(_ticks->text().toULong() + 1));
}


Window::Window() : _ticks("0")
{
	_layout->addWidget(_ticks);
}


Window::~Window() { }
