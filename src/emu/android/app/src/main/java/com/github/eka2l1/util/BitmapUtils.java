/*
 * Copyright (c) 2021 EKA2L1 Team
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

package com.github.eka2l1.util;


import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;


public class BitmapUtils {
    public static Bitmap SourceWithMergedSymbianMask(Bitmap source, Bitmap mask) {
        float[] nlf =  new float[] {
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,
                -1, 0, 0, 0, 255};

        ColorMatrix redToAlphaMat = new ColorMatrix(nlf);
        ColorMatrixColorFilter redToAlphaFilter = new ColorMatrixColorFilter(redToAlphaMat);

        Bitmap result = Bitmap.createBitmap(source.getWidth(), source.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas();

        Paint maskPaint = new Paint();
        maskPaint.setColorFilter(redToAlphaFilter);
        maskPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));

        canvas.setBitmap(result);
        canvas.drawBitmap(source, 0, 0, null);
        canvas.drawBitmap(mask, 0, 0, maskPaint);

        return result;
    }
}
