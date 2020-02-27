/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCDV_DVC_ALGO_H_
#define SCDV_DVC_ALGO_H_

#include <scdv/draw.h>
#include <scdv/log.h>

class CFbsDrawDeviceAlgorithm: public CFbsDrawDevice {
public:
	virtual TInt ScanLineBytes() const;
	virtual void MapColors(const TRect& aRect,const TRgb* aColors,TInt aNumPairs,TBool aMapForwards);
};

#endif
