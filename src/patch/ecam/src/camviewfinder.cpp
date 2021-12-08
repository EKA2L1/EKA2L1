/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <camimpl.h>
#include <dispatch.h>
#include <Log.h>

void CCameraPlugin::StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderDirectL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderDirectL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderBitmapsL(TSize& aSize) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderDirectL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderBitmapsL(TSize& aSize,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderBitmapsL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderL(TFormat aImageFormat,TSize& aSize) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderL(TFormat aImageFormat,TSize& aSize,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StopViewFinder() {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StopViewFinder)"));
}

TBool CCameraPlugin::ViewFinderActive() const {
    return EFalse;
}

void CCameraPlugin::SetViewFinderMirrorL(TBool aMirror) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called SetViewFinderMirrorL)"));
}

TBool CCameraPlugin::ViewFinderMirror() const {
    return EFalse;
}
