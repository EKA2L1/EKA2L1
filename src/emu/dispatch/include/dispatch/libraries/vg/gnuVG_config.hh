/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2016 by Anton Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __GNUVG_CONFIG_HH
#define __GNUVG_CONFIG_HH

#define GNUVG_MAX_COLOR_RAMP_STOPS 32

// Filters are unimplemented; report the OpenVG 1.1 minimum limits.
#define GNUVG_MAX_KERNEL_SIZE 7
#define GNUVG_MAX_SEPARABLE_KERNEL_SIZE 15
#define GNUVG_MAX_GAUSSIAN_STD_DEVIATION 16

#endif
