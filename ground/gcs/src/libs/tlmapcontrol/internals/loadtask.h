/**
******************************************************************************
*
* @file       loadtask.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
* @author     Tau Labs, http://taulabs.org, Copyright (C) 2013
* @brief      
* @see        The GNU Public License (GPL) Version 3
* @defgroup   TLMapWidget
* @{
* 
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
#ifndef LOADTASK_H
#define LOADTASK_H

#include <QString>
#include "../core/point.h"

using namespace core;
namespace internals
{
struct LoadTask
  {
     friend bool operator==(LoadTask const& lhs,LoadTask const& rhs);
  public:
    core::Point Pos; //Tile position in quadtile format
    int Zoom;        //Number of zoom levels, in quadtile format


    LoadTask(Point pos, int zoom)
     {
        Pos = pos;
        Zoom = zoom;
    }
    LoadTask()
    {
        Pos=core::Point(-1,-1);
        Zoom=-1;
    }
    bool HasValue()
    {
        return !(Zoom==-1);
    }

    QString ToString()const
     {
        return QString::number(Zoom) + " - " + Pos.ToString();
     }
  };
}
#endif // LOADTASK_H
