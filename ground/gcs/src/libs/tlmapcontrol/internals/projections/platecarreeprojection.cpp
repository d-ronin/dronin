/**
******************************************************************************
*
* @file       platecarreeprojection.cpp
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
#include "platecarreeprojection.h"


 
namespace projections {
PlateCarreeProjection::PlateCarreeProjection():MinLatitude(-85.05112878), MaxLatitude(85.05112878),MinLongitude(-180),
MaxLongitude(180), tileSize(512, 512)
{
}
Point PlateCarreeProjection::FromLatLngToPixel(double lat, double lng, const int &zoom)
{
    Point ret;// = Point.Empty;

    lat = bound(lat, MinLatitude, MaxLatitude);
    lng = bound(lng, MinLongitude, MaxLongitude);

    Size s = GetTileMatrixSizePixel(zoom);
    double mapSizeX = s.Width();
    //double mapSizeY = s.Height();

    double scale = 360.0 / mapSizeX;

    ret.SetY((qint64) round((90.0 - lat) / scale));
    ret.SetX((qint64) round((lng + 180.0) / scale));

    return ret;

}
internals::PointLatLng PlateCarreeProjection::FromPixelToLatLng(const qint64 &x,const qint64 &y,const int &zoom)
{
    internals::PointLatLng ret;// = internals::PointLatLng.Empty;

    Size s = GetTileMatrixSizePixel(zoom);
    double mapSizeX = s.Width();
    //double mapSizeY = s.Height();

    double scale = 360.0 / mapSizeX;

    ret.SetLat(90 - (y * scale));
    ret.SetLng((x * scale) - 180);

    return ret;
}

Size PlateCarreeProjection::TileSize() const
{
    return tileSize;
}
double PlateCarreeProjection::Axis() const
{
    return 6378137;
}
double PlateCarreeProjection::Flattening() const
{
    return (1.0 / 298.257223563);
}
Size PlateCarreeProjection::GetTileMatrixMaxXY(const int &zoom)
{
    int y = (int) pow(2, zoom);
    return Size((2*y) - 1, y - 1);
}

Size PlateCarreeProjection::GetTileMatrixMinXY(const int &zoom)
{
    Q_UNUSED(zoom);
    return Size(0, 0);
}
}
