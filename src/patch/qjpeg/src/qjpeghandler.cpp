/*
 * Copyright (c) 2026 EKA2L1 Team.
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

// A drop-in replacement for Qt's JPEG image format plugin that decodes on the
// host instead of running libjpeg under emulation. Qt applications reach it
// through the ordinary QImageReader plugin lookup, so nothing in the guest is
// aware of the substitution.

#include "dispatch.h"

#include <QtCore/QByteArray>
#include <QtCore/QIODevice>
#include <QtCore/QSize>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QImage>
#include <QtGui/QImageIOHandler>
#include <QtGui/QImageIOPlugin>

namespace {
    // Enough for the header of any JPEG this decodes: the dimensions live in a
    // SOF marker, which sits after the (rarely large) application segments.
    const int KHeaderPeekSize = 4096;

    bool ReadImageInfo(const QByteArray &aData, TImageDecodeInfo &aInfo) {
        if (aData.isEmpty()) {
            return false;
        }

        return EImageDecodeInfo(0, aData.constData(), static_cast<TUint32>(aData.size()), &aInfo) == KErrNone;
    }
}

class Eka2l1JpegHandler : public QImageIOHandler {
public:
    Eka2l1JpegHandler();

    bool canRead() const;
    bool read(QImage *aImage);

    QVariant option(ImageOption aOption) const;
    bool supportsOption(ImageOption aOption) const;

    static bool CanReadFrom(QIODevice *aDevice);

private:
    // The device is consumed by the first read; the peeked header is what
    // answers size queries without disturbing it.
    mutable QByteArray iPeeked;
    mutable bool iSizeKnown;
    mutable QSize iSize;

    const QByteArray &Peek() const;
};

Eka2l1JpegHandler::Eka2l1JpegHandler()
    : iSizeKnown(false) {
}

bool Eka2l1JpegHandler::CanReadFrom(QIODevice *aDevice) {
    if (!aDevice) {
        return false;
    }

    const QByteArray header = aDevice->peek(2);
    return (header.size() == 2) && (static_cast<unsigned char>(header[0]) == 0xFF)
        && (static_cast<unsigned char>(header[1]) == 0xD8);
}

const QByteArray &Eka2l1JpegHandler::Peek() const {
    if (iPeeked.isEmpty() && device()) {
        iPeeked = device()->peek(KHeaderPeekSize);
    }

    return iPeeked;
}

bool Eka2l1JpegHandler::canRead() const {
    if (!device()) {
        return false;
    }

    if (CanReadFrom(device())) {
        setFormat("jpeg");
        return true;
    }

    return false;
}

bool Eka2l1JpegHandler::read(QImage *aImage) {
    if (!aImage || !device()) {
        return false;
    }

    const QByteArray data = device()->readAll();
    TImageDecodeInfo info;

    if (!ReadImageInfo(data, info) || (info.iWidth == 0) || (info.iHeight == 0)) {
        return false;
    }

    // Format_RGB32 keeps rows 32-bit aligned and matches what the host writes
    // for EImageDecodePixelFormatBgra8888 on a little-endian guest.
    QImage decoded(static_cast<int>(info.iWidth), static_cast<int>(info.iHeight), QImage::Format_RGB32);

    if (decoded.isNull()) {
        return false;
    }

    const TInt err = EImageDecode(0, data.constData(), static_cast<TUint32>(data.size()),
        decoded.bits(), static_cast<TUint32>(decoded.numBytes()),
        static_cast<TUint32>(decoded.bytesPerLine()), EImageDecodePixelFormatBgra8888);

    if (err != KErrNone) {
        return false;
    }

    *aImage = decoded;
    return true;
}

QVariant Eka2l1JpegHandler::option(ImageOption aOption) const {
    if (aOption == Size) {
        if (!iSizeKnown) {
            TImageDecodeInfo info;

            if (ReadImageInfo(Peek(), info)) {
                iSize = QSize(static_cast<int>(info.iWidth), static_cast<int>(info.iHeight));
                iSizeKnown = true;
            }
        }

        if (iSizeKnown) {
            return iSize;
        }
    }

    if (aOption == ImageFormat) {
        return QImage::Format_RGB32;
    }

    return QVariant();
}

bool Eka2l1JpegHandler::supportsOption(ImageOption aOption) const {
    return (aOption == Size) || (aOption == ImageFormat);
}

class Eka2l1JpegPlugin : public QImageIOPlugin {
    Q_OBJECT

public:
    QStringList keys() const;
    Capabilities capabilities(QIODevice *aDevice, const QByteArray &aFormat) const;
    QImageIOHandler *create(QIODevice *aDevice, const QByteArray &aFormat = QByteArray()) const;
};

QStringList Eka2l1JpegPlugin::keys() const {
    return QStringList() << QLatin1String("jpg") << QLatin1String("jpeg");
}

QImageIOPlugin::Capabilities Eka2l1JpegPlugin::capabilities(QIODevice *aDevice, const QByteArray &aFormat) const {
    if ((aFormat == "jpeg") || (aFormat == "jpg")) {
        return Capabilities(CanRead);
    }

    if (!aFormat.isEmpty()) {
        return 0;
    }

    if (!aDevice || !aDevice->isOpen() || !aDevice->isReadable()) {
        return 0;
    }

    return Eka2l1JpegHandler::CanReadFrom(aDevice) ? Capabilities(CanRead) : Capabilities(0);
}

QImageIOHandler *Eka2l1JpegPlugin::create(QIODevice *aDevice, const QByteArray &aFormat) const {
    QImageIOHandler *handler = new Eka2l1JpegHandler();
    handler->setDevice(aDevice);
    handler->setFormat(aFormat.isEmpty() ? QByteArray("jpeg") : aFormat);

    return handler;
}

Q_EXPORT_PLUGIN2(qjpeg, Eka2l1JpegPlugin)

#include "qjpeghandler.moc"
