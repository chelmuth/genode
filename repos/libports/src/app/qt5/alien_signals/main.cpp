/*
 * \brief   Alien signals in Qt5
 * \author  Christian Helmuth
 * \date    2015-01-14
 */

/* Qt5 includes */
#include <QApplication>
#include <QFrame>

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <timer_session/connection.h>

/* local includes */
#include "window.h"


struct Ticker : Genode::Thread<0x4000>
{
	Timer::Connection timer;

	QMember<Proxy> proxy;

	Ticker(Window *win) : Genode::Thread<0x4000>("alien")
	{
		QObject::connect(proxy, SIGNAL(tick()),
		                 win,   SLOT(tick()),
		                 Qt::QueuedConnection);

		start();
	}

	void entry() override
	{
		PINF("Hello from your friendly alien.");

		while (true) {
			timer.msleep(1000);
			PINF("tick");
			proxy->tick();
		}
	}
};


int main(int argc, char *argv[])
{
	static QApplication app(argc, argv);

	Window *win = new Window;

	win->setGeometry(300, 100, 60, 40);
	win->show();

	static Ticker ticker(win);

	app.connect(&app, SIGNAL(lastWindowClosed()), SLOT(quit()));

	return app.exec();
}
