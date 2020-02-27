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

#include "drawdvcalgo.h"

/**
 * \brief Get number of bits per pixel of each display mode.
 */
static TInt GetBppFromDisplayMode(TDisplayMode aMode) {
	switch (aMode) {
		case EGray2:
			return 1;
	
		case EGray4:
			return 2;
			
		case EGray16:
		case EColor16:
			return 4;
			
		case EGray256:
		case EColor256:
			return 8;
		
		case EColor4K:
			return 12;
		
		case EColor64K:
			return 16;
		
		case EColor16M:
			return 24;
			
		case EColor16MA:
		case EColor16MAP:
		case EColor16MU:
			return 32;
		
		default:
			break;
	}
	
	return 32;
}

/**
 * \brief Replace target colors with other specified colors.
 * 
 * For example, you can replace a green color and replace them with yellow. Use this algorithm
 * to do that to multiple pixels and multiple colors.
 * 
 * \param aRect 		The target rectangle to map colors.
 * \param aColors		The color pairs. 2 colors create one pair, one is the color to be replaced,
 * 						one is the color to get replaced.
 * 	
 * \param aNumPairs 	Number of color pairs.
 * \param aMapForward	True if the color to be replaced is the first one.
 */
void CFbsDrawDeviceAlgorithm::MapColors(const TRect& aRect,const TRgb* aColors,TInt aNumPairs,TBool aMapForwards) {
	// Iterate through pixels. Order doesnt matter.
	for (TInt x = aRect.iTl.iX; x <= aRect.iBr.iX; x++) {
		for (TInt y = aRect.iTl.iY; y <= aRect.iBr.iY; y++) {
			const TRgb pixel = ReadPixel(x, y);
			const TInt targetToSwapIndex = static_cast<TInt>(aMapForwards);
			
			for (TInt pairIte = 0; pairIte < aNumPairs; pairIte++) {
				// Iterate through pairs to find match one. Then swap them by rewriting.
				if (aColors[(pairIte << 1) + targetToSwapIndex] == pixel) {
					WriteRgb(x, y, aColors[(pairIte << 1) + (1 - targetToSwapIndex)], CGraphicsContext::EDrawModeAND);
				}
			}
		}
	}
}

/**
 * \brief Query how many bytes a line of bitmap data consumes.
 */
TInt CFbsDrawDeviceAlgorithm::ScanLineBytes() const {
	const TInt bpp = GetBppFromDisplayMode(DisplayMode());
	return ((SizeInPixels().iWidth * bpp) + 31) >> 6;
}
