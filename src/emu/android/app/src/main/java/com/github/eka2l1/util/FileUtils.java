/*
 * Copyright (c) 2020 EKA2L1 Team
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

import android.content.Context;
import android.database.Cursor;
import android.icu.util.Output;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.util.Log;

import androidx.activity.result.contract.ActivityResultContract;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

public class FileUtils {

    private static String TAG = FileUtils.class.getName();

    public static void copyFiles(File src, File dst, FilenameFilter filter) {
        if (!dst.exists() && !dst.mkdirs()) {
            Log.e(TAG, "copyFiles() failed create dir: " + dst);
            return;
        }
        File[] list = src.listFiles(filter);
        if (list == null) {
            return;
        }
        for (File file : list) {
            File to = new File(dst, file.getName());
            if (file.isDirectory()) {
                copyFiles(src, to, filter);
            } else {
                try {
                    copyFileUsingChannel(file, to);
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public static String getFilenameFromURI(Context context, String path) {
        Cursor returnCursor = context.getContentResolver().query(Uri.parse(path), null, null, null, null);
        assert returnCursor != null;
        int nameIndex = returnCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
        returnCursor.moveToFirst();
        String name = returnCursor.getString(nameIndex);
        returnCursor.close();
        return name;
    }

    public static String getFilenameFromPath(Context context, String path) {
        String folderName;

        if (path.startsWith("/")) {
            folderName = (new File(path)).getName();
        } else {
            folderName = getFilenameFromURI(context, path);
        }

        return folderName;
    }

    public static void copyFileFromURI(Context context, String pointedURI, File dest) throws IOException {
        Uri uri = Uri.parse(pointedURI);
        copyFileFromInputStream(context.getContentResolver().openInputStream(uri), dest);
    }

    public static void copyFileFromInputStream(InputStream source, File dest) throws IOException {
        byte[] buffer = new byte[2048];
        int read = 0;

        OutputStream stream = new FileOutputStream(dest);

        while ((read = source.read(buffer)) != -1) {
            stream.write(buffer, 0, read);
        }
    }

    public static void copyFileUsingChannel(File source, File dest) throws IOException {
        if (!source.exists()) {
            return;
        }
        try (FileChannel sourceChannel = new FileInputStream(source).getChannel();
             FileChannel destChannel = new FileOutputStream(dest).getChannel()) {
            destChannel.transferFrom(sourceChannel, 0, sourceChannel.size());
        }
    }

    public static boolean deleteDirectory(File dir) {
        if (dir.isDirectory()) {
            File[] listFiles = dir.listFiles();
            if (listFiles != null && listFiles.length != 0) {
                for (File file : listFiles) {
                    deleteDirectory(file);
                }
            }
        }
        if (!dir.delete() && dir.exists()) {
            return false;
        }
        return true;
    }

    public static boolean copyAssetFolder(Context context, String srcName, String dstName) {
        try {
            boolean result;
            String fileList[] = context.getAssets().list(srcName);
            if (fileList == null) return false;

            if (fileList.length == 0) {
                result = copyAssetFile(context, srcName, dstName);
            } else {
                File file = new File(dstName);
                result = file.mkdirs();
                for (String filename : fileList) {
                    result &= copyAssetFolder(context, srcName + File.separator + filename,
                            dstName + File.separator + filename);
                }
            }
            return result;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    public static boolean copyAssetFile(Context context, String srcName, String dstName) {
        try {
            InputStream in = context.getAssets().open(srcName);
            File outFile = new File(dstName);
            OutputStream out = new FileOutputStream(outFile);
            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            in.close();
            out.close();
            return true;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    public static void writeToFile(String data, File dest) {
        try {
            FileOutputStream fos = new FileOutputStream(dest);
            OutputStreamWriter outputStreamWriter = new OutputStreamWriter(fos);
            outputStreamWriter.write(data);
            outputStreamWriter.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static String readFromFile(File source) {
        String result = "";
        try {
            FileInputStream fis = new FileInputStream(source);
            InputStreamReader inputStreamReader = new InputStreamReader(fis);
            BufferedReader bufferedReader = new BufferedReader(inputStreamReader);
            String receiveString;
            StringBuilder stringBuilder = new StringBuilder();

            while ((receiveString = bufferedReader.readLine()) != null) {
                stringBuilder.append(receiveString);
            }

            fis.close();
            result = stringBuilder.toString();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return result;
    }

    public static boolean isExternalStorageLegacy() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.Q || Environment.isExternalStorageLegacy();
    }

    public static ActivityResultContract<String[],String> getFilePicker() {
        if (isExternalStorageLegacy()) {
            return new PickFileResultContract();
        } else {
            return new SAFFileResultContract();
        }
    }

    public static ActivityResultContract<Void,String> getDirPicker() {
        if (isExternalStorageLegacy()) {
            return new PickDirResultContract();
        } else {
            return new SAFDirResultContract();
        }
    }
}
