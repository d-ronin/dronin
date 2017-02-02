/**
 ******************************************************************************
 * @addtogroup TauLabsModules Tau Labs Modules
 * @{
 * @addtogroup Control Control Module
 * @{
 *
 * @file       transmitter_control.h
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2015-2016
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2013-2015
 * @brief      Process transmitter inputs and use as control source
 *
 * @see        The GNU Public License (GPL) Version 3
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
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 */

#ifndef TRANSMITTER_CONTROL_H
#define TRANSMITTER_CONTROL_H

#include "stdint.h"
#include "manualcontrolcommand.h"
#include "manualcontrolsettings.h"
#include "stabilizationdesired.h"
#include "stabilizationsettings.h"

/*
 * These are assumptions we make in the flight code about the order of settings and their consistency between
 * objects.  Please keep this synchronized to the UAVObjects
 */
#define assumptions1 ( \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_DISABLED      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_DISABLED)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_MANUAL      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_MANUAL)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_RATE      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_RATE)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_AXISLOCK  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_AXISLOCK)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_WEAKLEVELING      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_WEAKLEVELING)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_ATTITUDE  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)      \
)

#define assumptions3 ( \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_DISABLED      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_DISABLED)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_MANUAL      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_MANUAL)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_RATE      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_RATE)      && \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_AXISLOCK  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_AXISLOCK)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_WEAKLEVELING      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_WEAKLEVELING)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION2SETTINGS_ATTITUDE  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)     \
)

#define assumptions5 ( \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_DISABLED      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_DISABLED)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_MANUAL      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_MANUAL)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_RATE      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_RATE)      && \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_AXISLOCK  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_AXISLOCK)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_WEAKLEVELING      == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_WEAKLEVELING)       && \
((int)MANUALCONTROLSETTINGS_STABILIZATION3SETTINGS_ATTITUDE  == (int)STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)     \
)

#define assumptions_flightmode ( \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_MANUAL == (int) FLIGHTSTATUS_FLIGHTMODE_MANUAL) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_ACRO == (int) FLIGHTSTATUS_FLIGHTMODE_ACRO) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_LEVELING == (int) FLIGHTSTATUS_FLIGHTMODE_LEVELING) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_HORIZON == (int) FLIGHTSTATUS_FLIGHTMODE_HORIZON) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_AXISLOCK == (int) FLIGHTSTATUS_FLIGHTMODE_AXISLOCK) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_VIRTUALBAR == (int) FLIGHTSTATUS_FLIGHTMODE_VIRTUALBAR) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_STABILIZED1 == (int) FLIGHTSTATUS_FLIGHTMODE_STABILIZED1) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_STABILIZED2 == (int) FLIGHTSTATUS_FLIGHTMODE_STABILIZED2) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_STABILIZED3 == (int) FLIGHTSTATUS_FLIGHTMODE_STABILIZED3) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_POSITIONHOLD == (int) FLIGHTSTATUS_FLIGHTMODE_POSITIONHOLD) && \
( (int)MANUALCONTROLSETTINGS_FLIGHTMODEPOSITION_RETURNTOHOME == (int) FLIGHTSTATUS_FLIGHTMODE_RETURNTOHOME) \
)

#define assumptions_channelcount ( \
( (int)MANUALCONTROLCOMMAND_CHANNEL_NUMELEM == (int)MANUALCONTROLSETTINGS_CHANNELGROUPS_NUMELEM ) && \
( (int)MANUALCONTROLCOMMAND_CHANNEL_NUMELEM == (int)MANUALCONTROLSETTINGS_CHANNELNUMBER_NUMELEM ) && \
( (int)MANUALCONTROLCOMMAND_CHANNEL_NUMELEM == (int)MANUALCONTROLSETTINGS_CHANNELMIN_NUMELEM ) && \
( (int)MANUALCONTROLCOMMAND_CHANNEL_NUMELEM == (int)MANUALCONTROLSETTINGS_CHANNELMAX_NUMELEM ) && \
( (int)MANUALCONTROLCOMMAND_CHANNEL_NUMELEM == (int)MANUALCONTROLSETTINGS_CHANNELNEUTRAL_NUMELEM ) )

DONT_BUILD_IF(!(assumptions1), TransmitterControlAssumptions1);
DONT_BUILD_IF(!(assumptions3), TransmitterControlAssumptions3);
DONT_BUILD_IF(!(assumptions5), TransmitterControlAssumptions5);
DONT_BUILD_IF(!(assumptions_flightmode), TransmitterControlAssumptions_fm);
DONT_BUILD_IF(!(assumptions_channelcount), TransmitterControlAssumptions_cc);

#define NEQ(a,b) (((int) (a)) != ((int) (b)))

DONT_BUILD_IF(
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_ROLL) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, STABILIZATIONSETTINGS_MANUALRATE_ROLL) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, STABILIZATIONSETTINGS_RATEEXPO_ROLL) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, STABILIZATIONSETTINGS_HORIZONEXPO_ROLL) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, STABILIZATIONSETTINGS_DEADBANDWIDTH_ROLL) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL, STABILIZATIONSETTINGS_DEADBANDSLOPE_ROLL),
		ROLLIndicesMismatch);

DONT_BUILD_IF(
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_PITCH) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, STABILIZATIONSETTINGS_MANUALRATE_PITCH) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, STABILIZATIONSETTINGS_RATEEXPO_PITCH) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, STABILIZATIONSETTINGS_HORIZONEXPO_PITCH) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, STABILIZATIONSETTINGS_DEADBANDWIDTH_PITCH) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH, STABILIZATIONSETTINGS_DEADBANDSLOPE_PITCH),
		PITCHIndicesMismatch);

DONT_BUILD_IF(
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, MANUALCONTROLSETTINGS_STABILIZATION1SETTINGS_YAW) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, STABILIZATIONSETTINGS_MANUALRATE_YAW) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, STABILIZATIONSETTINGS_RATEEXPO_YAW) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, STABILIZATIONSETTINGS_HORIZONEXPO_YAW) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, STABILIZATIONSETTINGS_DEADBANDWIDTH_YAW) ||
		NEQ(STABILIZATIONDESIRED_STABILIZATIONMODE_YAW, STABILIZATIONSETTINGS_DEADBANDSLOPE_YAW),
		YAWIndicesMismatch);

//! Initialize the transmitter control mode
int32_t transmitter_control_initialize();

//! Process inputs and arming
int32_t transmitter_control_update();

//! Select and use transmitter control
int32_t transmitter_control_select(bool reset_controller);

//! Query what the flight mode _would_ be if this is selected
uint8_t transmitter_control_get_flight_mode();

//! Get any control events
enum control_events transmitter_control_get_events();

#endif /* TRANSMITTER_CONTROL_H */

/**
 * @}
 * @}
 */
