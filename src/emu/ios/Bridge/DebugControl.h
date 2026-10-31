// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
@interface EKA2L1ControlRequest : NSObject
@property(nonatomic, copy) NSString *method;
@property(nonatomic, copy) NSArray<NSString *> *path;
@property(nonatomic, copy) NSDictionary<NSString *, NSString *> *query;
@property(nonatomic, copy) NSDictionary<NSString *, NSString *> *headers;
@property(nonatomic, copy) NSData *body;
@property(nonatomic, copy, nullable) void (^progress)(double);
@end
@interface EKA2L1ControlResponse : NSObject
@property(nonatomic) NSInteger status;
@property(nonatomic, copy) NSString *contentType;
@property(nonatomic, copy) NSDictionary<NSString *, NSString *> *headers;
@property(nonatomic, copy) NSData *body;
@end
@interface EKA2L1ControlServer : NSObject
- (BOOL)startWithPort:(uint16_t)port
             handler:(void (^)(EKA2L1ControlRequest *, void (^)(EKA2L1ControlResponse *)))handler
               error:(NSString * _Nullable * _Nullable)error;
- (void)stop;
@end
NS_ASSUME_NONNULL_END
