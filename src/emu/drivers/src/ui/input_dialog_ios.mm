// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <drivers/ui/input_dialog.h>

#import <UIKit/UIKit.h>

namespace {
    UIAlertController *g_active_alert = nil;

    NSString *to_ns_string(const std::u16string &str) {
        return [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar *>(str.data())
                                             length:str.size()];
    }

    std::u16string to_u16_string(NSString *str) {
        std::u16string result(str.length, u'\0');
        [str getCharacters:reinterpret_cast<unichar *>(result.data()) range:NSMakeRange(0, str.length)];
        return result;
    }

    UIViewController *top_view_controller() {
        UIScene *scene = UIApplication.sharedApplication.connectedScenes.anyObject;
        if (!scene || scene.activationState != UISceneActivationStateForegroundActive) {
            for (UIScene *candidate in UIApplication.sharedApplication.connectedScenes) {
                if (candidate.activationState == UISceneActivationStateForegroundActive) {
                    scene = candidate;
                    break;
                }
            }
        }

        UIWindowScene *window_scene = [scene isKindOfClass:UIWindowScene.class] ? (UIWindowScene *)scene : nil;
        UIViewController *controller = window_scene.keyWindow.rootViewController;
        while (controller.presentedViewController) {
            controller = controller.presentedViewController;
        }
        return controller;
    }

    void close_input_view_on_main() {
        if (g_active_alert) {
            [g_active_alert dismissViewControllerAnimated:YES completion:nil];
            g_active_alert = nil;
        }
    }
}

namespace eka2l1::drivers::ui {
    bool open_input_view(const std::u16string &initial_text, const int max_len,
        input_dialog_complete_callback complete_callback) {
        dispatch_async(dispatch_get_main_queue(), ^{
            close_input_view_on_main();

            UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Input"
                                                                           message:nil
                                                                    preferredStyle:UIAlertControllerStyleAlert];
            [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
                field.text = to_ns_string(initial_text);
                field.clearButtonMode = UITextFieldViewModeWhileEditing;
            }];

            __weak UIAlertController *weak_alert = alert;
            UIAlertAction *ok = [UIAlertAction actionWithTitle:@"OK"
                                                         style:UIAlertActionStyleDefault
                                                       handler:^(__unused UIAlertAction *action) {
                NSString *text = weak_alert.textFields.firstObject.text ?: @"";
                if (max_len > 0 && text.length > static_cast<NSUInteger>(max_len)) {
                    text = [text substringToIndex:static_cast<NSUInteger>(max_len)];
                }
                const std::u16string result = to_u16_string(text);
                if (complete_callback) {
                    complete_callback(result);
                }
                g_active_alert = nil;
            }];
            UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                            style:UIAlertActionStyleCancel
                                                          handler:^(__unused UIAlertAction *action) {
                if (complete_callback) {
                    complete_callback(initial_text);
                }
                g_active_alert = nil;
            }];
            [alert addAction:cancel];
            [alert addAction:ok];

            g_active_alert = alert;
            [top_view_controller() presentViewController:alert animated:YES completion:nil];
        });
        return true;
    }

    void close_input_view() {
        dispatch_async(dispatch_get_main_queue(), ^{
            close_input_view_on_main();
        });
    }

    void show_yes_no_dialog(const std::u16string &text, const std::u16string &button1_text,
        const std::u16string &button2_text, yes_no_dialog_complete_callback complete_callback) {
        dispatch_async(dispatch_get_main_queue(), ^{
            close_input_view_on_main();

            UIAlertController *alert = [UIAlertController alertControllerWithTitle:nil
                                                                           message:to_ns_string(text)
                                                                    preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:to_ns_string(button1_text)
                                                      style:UIAlertActionStyleDefault
                                                    handler:^(__unused UIAlertAction *action) {
                if (complete_callback) {
                    complete_callback(0);
                }
                g_active_alert = nil;
            }]];
            [alert addAction:[UIAlertAction actionWithTitle:to_ns_string(button2_text)
                                                      style:UIAlertActionStyleCancel
                                                    handler:^(__unused UIAlertAction *action) {
                if (complete_callback) {
                    complete_callback(1);
                }
                g_active_alert = nil;
            }]];

            g_active_alert = alert;
            [top_view_controller() presentViewController:alert animated:YES completion:nil];
        });
    }
}
