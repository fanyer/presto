/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4Utils.h"

#include <KDE/KApplication>
#include <KDE/KDialog>
#include <KDE/KWindowSystem>
#include <QX11Info>

#include <X11/Xutil.h> // XClassHint 

namespace Kde4Utils
{
	class WinIdEmbedder: public QObject
	{
		public:
			WinIdEmbedder(KApplication* application, WId winId)
			: QObject(application), id(winId)
			{
				if (application)
					application->installEventFilter(this);
			}
		protected:
			bool eventFilter(QObject *o, QEvent *e);
		private:
			WId id;
	};

	bool WinIdEmbedder::eventFilter(QObject *o, QEvent *e)
	{
		if (e->type() == QEvent::Show && o->isWidgetType()
				  && o->inherits("QDialog"))
		{
			QWidget *w = static_cast<QWidget*>(o);
			if (id)
				KWindowSystem::setMainWindow(w, id);
			deleteLater(); // WinIdEmbedder is not needed anymore after the first dialog was shown
			return false;
		}
		return QObject::eventFilter(o, e);
	}
};

int Kde4Utils::RunDialog(QDialog* dialog, X11Types::Window parent)
{
	WinIdEmbedder* embedder = new WinIdEmbedder(KApplication::kApplication(), parent); // will delete itself
	(void)embedder; // to prevent warnings

	return dialog->exec();
}


void Kde4Utils::SetResourceName(QWidget* widget, const char* name)
{
	if (!widget)
		return;

	char* copy = name ? strdup(name) : 0; // avoid compile error wrt const assignment

	XClassHint hint;
	char opera[6];
	strcpy(opera, "Opera");
	hint.res_class = opera;
	hint.res_name  = copy ? copy : hint.res_class;
	XSetClassHint(QX11Info::display(), widget->winId(), &hint);

	free(copy);
}
