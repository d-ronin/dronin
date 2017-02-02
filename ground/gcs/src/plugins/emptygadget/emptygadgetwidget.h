/**
 ******************************************************************************
 *
 * @file       emptygadgetwidget.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup EmptyGadgetPlugin Empty Gadget Plugin
 * @{
 * @brief A place holder gadget plugin 
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

#ifndef EMPTYGADGETWIDGET_H_
#define EMPTYGADGETWIDGET_H_

#include <QLabel>

class EmptyGadgetWidget : public QLabel
{
    Q_OBJECT

public:
    EmptyGadgetWidget(QWidget *parent = 0);
   ~EmptyGadgetWidget();

private:
};

#endif /* EMPTYGADGETWIDGET_H_ */
