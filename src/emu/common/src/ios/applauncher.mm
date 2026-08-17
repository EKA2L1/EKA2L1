// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <common/applauncher.h>

#import <UIKit/UIKit.h>

namespace eka2l1::common {
    bool launch_browser(const std::string &url) {
        NSString *str = [NSString stringWithUTF8String:url.c_str()];
        if (str.length == 0) {
            return false;
        }
        NSURL *nsUrl = [NSURL URLWithString:str];
        if (!nsUrl) {
            return false;
        }
        UIApplication *app = UIApplication.sharedApplication;
        if (![app canOpenURL:nsUrl]) {
            return false;
        }
        [app openURL:nsUrl options:@{} completionHandler:nil];
        return true;
    }
}
