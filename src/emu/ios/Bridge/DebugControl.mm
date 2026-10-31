// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#import "DebugControl.h"
#import <TargetConditionals.h>

#if TARGET_OS_SIMULATOR
#include <control/service.h>
#include <memory>

@implementation EKA2L1ControlRequest
@end
@implementation EKA2L1ControlResponse
@end

@implementation EKA2L1ControlServer {
    std::unique_ptr<eka2l1::control::server> _server;
    std::shared_ptr<eka2l1::control::service> _service;
}
- (BOOL)startWithPort:(uint16_t)port
             handler:(void (^)(EKA2L1ControlRequest *, void (^)(EKA2L1ControlResponse *)))handler
               error:(NSString **)error {
    [self stop];
    void (^block)(EKA2L1ControlRequest *, void (^)(EKA2L1ControlResponse *)) = [handler copy];
    _service = std::make_shared<eka2l1::control::service>([block](eka2l1::control::request req, eka2l1::control::completion done) {
        @autoreleasepool {
            auto request = [EKA2L1ControlRequest new];
            request.method = @(req.method.c_str());
            auto path = [NSMutableArray new];
            for (const auto &part : req.path) [path addObject:@(part.c_str())];
            request.path = path;
            auto query = [NSMutableDictionary new];
            for (const auto &[key, value] : req.query) query[@(key.c_str())] = @(value.c_str());
            request.query = query;
            auto headers = [NSMutableDictionary new];
            for (const auto &[key, value] : req.headers) headers[@(key.c_str())] = @(value.c_str());
            request.headers = headers;
            request.body = [NSData dataWithBytes:req.body.data() length:req.body.size()];
            if (req.progress) {
                auto progress = req.progress;
                request.progress = ^(double fraction) { progress(fraction); };
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                block(request, ^(EKA2L1ControlResponse *response) {
                    eka2l1::control::response result;
                    result.status = static_cast<int>(response.status);
                    result.content_type = response.contentType.UTF8String;
                    result.body.assign(static_cast<const char *>(response.body.bytes), response.body.length);
                    for (NSString *key in response.headers) result.headers[key.UTF8String] = response.headers[key].UTF8String;
                    done(std::move(result));
                });
            });
        }
    });
    _server = std::make_unique<eka2l1::control::server>();
    std::string message;
    auto service = _service;
    if (!_server->start(port, [service](auto req, auto done) { service->handle(std::move(req), std::move(done)); }, message)) {
        if (error) *error = @(message.c_str());
        [self stop];
        return NO;
    }
    return YES;
}
- (void)stop {
    if (_server) _server->stop();
    if (_service) _service->stop();
    _server.reset();
    _service.reset();
}
- (void)dealloc { [self stop]; }
@end

#endif
