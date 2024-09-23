/*
 * \brief  Tiled-WM test: utilities
 * \author Christian Helmuth
 * \date   2018-09-26
 */

/*
 * Copyright (C) 2018-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__TILED_WM__UTIL_H_
#define _TEST__TILED_WM__UTIL_H_

/* Genode includes */
#include <base/signal.h>
#include <base/thread.h>
#include <base/log.h>
#include <util/string.h>

/* Qt includes */
#include <QApplication>
#include <QObject>
#include <QFile>
#include <QDebug>
#include <QFrame>

/* Qoost includes */
#include <qoost/qmember.h>

/* Libc includes */
#include <libc/component.h>

/* qt5_component includes */
#include <qt5_component/qpa_init.h>

using Name = Genode::String<32>;


/*
 * TODO move to libc
 */

#include <base/entrypoint.h>

/* libc internal */
#include <internal/thread_create.h> /* Libc::pthread_create() */

namespace Libc {
	class Entrypoint;

	using namespace Genode;
}

class Libc::Entrypoint : public Genode::Entrypoint
{
	private:

		Blockade _construction_finalized { };

		pthread_t _pthread { };

		void _finalize_construction()
		{
			Thread &myself = *Thread::myself();

			int dummy = 0;
			Libc::pthread_create_from_thread(&_pthread, myself, &dummy);

			_construction_finalized.wakeup();
		}

		Signal_handler<Entrypoint> _finalize_construction_sigh {
			*this, *this, &Entrypoint::_finalize_construction };

	public:

		Entrypoint(Env &env, size_t stack_size, char const *name,
		           Affinity::Location location)
		:
			Genode::Entrypoint(env, stack_size, name, location)
		{
			Signal_transmitter(_finalize_construction_sigh).submit();

			_construction_finalized.block();
		}
};


/*
 * Genode signal to queued Qt signal proxy
 */
class Genode_signal_proxy : public QObject,
                            public Genode::Signal_handler<Genode_signal_proxy>
{
	Q_OBJECT

	public:

		Genode_signal_proxy(Genode::Entrypoint &sig_ep)
		:
			Genode::Signal_handler<Genode_signal_proxy>(
				sig_ep, *this, &Genode_signal_proxy::handle_genode_signal)
		{
			connect(this, SIGNAL(internal_signal()),
			        this, SIGNAL(signal()),
			        Qt::QueuedConnection);
		}

	/* called by signal handler / emits internal signal in context of
	 * signal-entrypoint thread */
	void handle_genode_signal() { Q_EMIT internal_signal(); }

	Q_SIGNALS:

		/* internal_signal() is Qt::QueuedConnection to signal() */
		void internal_signal();

		/* finally signal() is emitted in the context of the Qt main thread */
		void signal();
};


/*
 * Qt initialization
 */

static inline QApplication & qt5_initialization(Libc::Env &env)
{
	qpa_init(env);

	char const *argv[] = { "qt5_app", 0 };
	int argc = sizeof(argv)/sizeof(*argv);

	static QApplication app(argc, (char**)argv);

	QFile file(":style.qss");
	if (!file.open(QFile::ReadOnly)) {
		qWarning() << "Warning:" << file.errorString()
			<< "opening file" << file.fileName();
	} else {
		qApp->setStyleSheet(QLatin1String(file.readAll()));
	}

	app.connect(&app, SIGNAL(lastWindowClosed()), SLOT(quit()));

	return app;
}


/*
 * Widget utilities
 */

struct Spacer : QFrame
{
	Q_OBJECT public:

	Spacer(QString const &style_id = "") { setObjectName(style_id); }

	~Spacer() { }
};

#endif /* _TEST__TILED_WM__UTIL_H_ */
