/**
 ******************************************************************************
 *
 * @file       hitlgadget.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 *
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup HITLPlugin HITL Plugin
 * @{
 * @brief The Hardware In The Loop plugin
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 */

#ifndef HITL_H
#define HITL_H

#include <coreplugin/iuavgadget.h>
#include "hitlwidget.h"

class IUAVGadget;
class QWidget;
class QString;

using namespace Core;

class HITLGadget : public Core::IUAVGadget
{
    Q_OBJECT
public:
	HITLGadget(QString classId, HITLWidget *widget, QWidget *parent = 0);
	~HITLGadget();

    QWidget *widget() { return m_widget; }
    void loadConfiguration(IUAVGadgetConfiguration* config);

signals:
	void changeConfiguration();

private:
	HITLWidget* m_widget;
};


#endif // HITL_H
