/*
 * \brief   Alien signals in Qt5
 * \author  Christian Helmuth
 * \date    2015-01-14
 */

#ifndef _ALIEN_SIGNALS__WINDOW_H_
#define _ALIEN_SIGNALS__WINDOW_H_

/* Qt5 includes */
#include <QFrame>
#include <QLabel>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>


struct Proxy : QObject
{
	Q_OBJECT public:

	Q_SIGNAL void tick();
};


class Window : public Compound_widget<QFrame, QVBoxLayout, 10>
{
	Q_OBJECT

	private:

		QMember<QLabel> _ticks;

	public:

		Window();
		~Window();

	public Q_SLOTS:

		void tick();
};

#endif /* _ALIEN_SIGNALS__WINDOW_H_ */
