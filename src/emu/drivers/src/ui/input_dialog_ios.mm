// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <drivers/ui/input_dialog.h>
#include <drivers/ui/input_dialog_ios.h>
#include <memory>
#include <atomic>

#import <UIKit/UIKit.h>

namespace {
    UIAlertController *g_active_alert = nil;
    struct input_request {
        std::u16string text;
        int max_length;
        std::uint64_t generation;
        eka2l1::drivers::ui::input_dialog_complete_callback complete;
    };
    std::shared_ptr<input_request> g_input;
    bool g_automatic_input = true;
    std::atomic<std::uint64_t> g_input_generation{0};
    bool g_present_next_input = false;
    std::uint64_t g_input_owner = 0;
    std::function<void(bool)> g_input_available_callback;
    bool g_last_input_available = false;

    void notify_input_available() {
        const bool available = eka2l1::drivers::ui::is_input_available();
        if (available != g_last_input_available) {
            g_last_input_available = available;
            if (g_input_available_callback) {
                g_input_available_callback(available);
            }
        }
    }

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
        g_input.reset();
        if (g_active_alert) {
            [g_active_alert dismissViewControllerAnimated:YES completion:nil];
            g_active_alert = nil;
        }
        notify_input_available();
    }
}

namespace eka2l1::drivers::ui {
    void set_automatic_input_view(bool automatic) {
        dispatch_async(dispatch_get_main_queue(), ^{
            g_automatic_input = automatic;
            notify_input_available();
        });
    }

    void request_input_view(std::function<void()> activate_editor) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!is_input_available()) {
                return;
            }
            if (g_input) {
                present_input_view();
            } else {
                g_present_next_input = true;
                activate_editor();
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                    g_present_next_input = false;
                });
            }
        });
    }

    void set_input_available(std::uint64_t owner, bool available) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (available) {
                g_input_owner = owner;
            } else if (g_input_owner == owner) {
                g_input_owner = 0;
                g_present_next_input = false;
            }
            notify_input_available();
        });
    }

    void set_input_available_callback(std::function<void(bool)> callback) {
        dispatch_async(dispatch_get_main_queue(), ^{
            g_input_available_callback = callback;
            g_last_input_available = is_input_available();
            if (g_input_available_callback) {
                g_input_available_callback(g_last_input_available);
            }
        });
    }

    bool is_input_available() {
        return !g_automatic_input && g_input_owner != 0 && !g_active_alert;
    }

    void reset_input_view() {
        close_input_view();
        dispatch_async(dispatch_get_main_queue(), ^{
            g_input_owner = 0;
            g_present_next_input = false;
            notify_input_available();
        });
    }

    bool open_input_view(const std::u16string &initial_text, const int max_len,
        input_dialog_complete_callback complete_callback) {
        const std::u16string initial_text_copy = initial_text;
        const auto generation = ++g_input_generation;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation != g_input_generation) {
                return;
            }
            close_input_view_on_main();
            g_input = std::make_shared<input_request>(input_request{initial_text_copy, max_len, generation, complete_callback});
            if (g_automatic_input || g_present_next_input) {
                g_present_next_input = false;
                present_input_view();
            }
        });
        return true;
    }

    void present_input_view() {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!g_input || g_active_alert) {
                return;
            }
            const auto request = g_input;

            UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Input"
                                                                           message:nil
                                                                    preferredStyle:UIAlertControllerStyleAlert];
            [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
                field.text = to_ns_string(request->text);
                field.clearButtonMode = UITextFieldViewModeWhileEditing;
                field.autocapitalizationType = UITextAutocapitalizationTypeNone;
                field.autocorrectionType = UITextAutocorrectionTypeNo;
            }];

            __weak UIAlertController *weak_alert = alert;
            UIAlertAction *ok = [UIAlertAction actionWithTitle:@"OK"
                                                         style:UIAlertActionStyleDefault
                                                       handler:^(__unused UIAlertAction *action) {
                NSString *text = weak_alert.textFields.firstObject.text ?: @"";
                if (g_input != request || request->generation != g_input_generation) {
                    return;
                }
                if (request->max_length > 0 && text.length > static_cast<NSUInteger>(request->max_length)) {
                    text = [text substringToIndex:static_cast<NSUInteger>(request->max_length)];
                }
                const std::u16string result = to_u16_string(text);
                g_input.reset();
                g_active_alert = nil;
                notify_input_available();
                if (request->complete) {
                    request->complete(result);
                }
            }];
            UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                            style:UIAlertActionStyleCancel
                                                          handler:^(__unused UIAlertAction *action) {
                if (g_input != request || request->generation != g_input_generation) {
                    return;
                }
                g_active_alert = nil;
                notify_input_available();
                if (g_automatic_input) {
                    g_input.reset();
                    if (request->complete) {
                        request->complete(request->text);
                    }
                }
            }];
            [alert addAction:cancel];
            [alert addAction:ok];

            g_active_alert = alert;
            notify_input_available();
            [top_view_controller() presentViewController:alert animated:YES completion:nil];
        });
    }

    void close_input_view() {
        const auto generation = ++g_input_generation;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation == g_input_generation) {
                close_input_view_on_main();
            }
        });
    }

    void show_yes_no_dialog(const std::u16string &text, const std::u16string &button1_text,
        const std::u16string &button2_text, yes_no_dialog_complete_callback complete_callback) {
        const std::u16string text_copy = text;
        const std::u16string button1_text_copy = button1_text;
        const std::u16string button2_text_copy = button2_text;
        dispatch_async(dispatch_get_main_queue(), ^{
            close_input_view_on_main();

            UIAlertController *alert = [UIAlertController alertControllerWithTitle:nil
                                                                           message:to_ns_string(text_copy)
                                                                    preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:to_ns_string(button1_text_copy)
                                                      style:UIAlertActionStyleDefault
                                                    handler:^(__unused UIAlertAction *action) {
                if (complete_callback) {
                    complete_callback(0);
                }
                g_active_alert = nil;
                notify_input_available();
            }]];
            [alert addAction:[UIAlertAction actionWithTitle:to_ns_string(button2_text_copy)
                                                      style:UIAlertActionStyleCancel
                                                    handler:^(__unused UIAlertAction *action) {
                if (complete_callback) {
                    complete_callback(1);
                }
                g_active_alert = nil;
                notify_input_available();
            }]];

            g_active_alert = alert;
            notify_input_available();
            [top_view_controller() presentViewController:alert animated:YES completion:nil];
        });
    }
}
