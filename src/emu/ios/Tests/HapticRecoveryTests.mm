#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#import <UIKit/UIKit.h>

#include <drivers/hwrm/backend/vibration_ios.h>
#include <cassert>
#include <cmath>
#include <memory>

@interface TestHapticPlayer : NSObject
@property(nonatomic) BOOL loopEnabled;
@property(nonatomic) NSTimeInterval loopEnd;
@property(nonatomic) NSUInteger starts;
@property(nonatomic) NSUInteger stops;
- (BOOL)startAtTime:(NSTimeInterval)time error:(NSError **)error;
- (BOOL)stopAtTime:(NSTimeInterval)time error:(NSError **)error;
@end
@implementation TestHapticPlayer
- (BOOL)startAtTime:(NSTimeInterval)time error:(NSError **)error { ++_starts; return YES; }
- (BOOL)stopAtTime:(NSTimeInterval)time error:(NSError **)error { ++_stops; return YES; }
@end

@interface TestHapticEngine : NSObject
@property(nonatomic) BOOL playsHapticsOnly;
@property(nonatomic) BOOL failsToStart;
@property(nonatomic) BOOL throwsOnStart;
@property(nonatomic, strong) NSMutableArray<TestHapticPlayer *> *players;
@property(nonatomic, strong) CHHapticPattern *pattern;
- (BOOL)startAndReturnError:(NSError **)error;
- (void)stopWithCompletionHandler:(void (^)(NSError *))handler;
- (id<CHHapticAdvancedPatternPlayer>)createAdvancedPlayerWithPattern:(CHHapticPattern *)pattern error:(NSError **)error;
@end
@implementation TestHapticEngine
- (instancetype)init {
    if ((self = [super init])) _players = [NSMutableArray array];
    return self;
}
- (BOOL)startAndReturnError:(NSError **)error {
    assert(error != NULL);
    if (_throwsOnStart) [NSException raise:@"CHHapticTestException" format:@"engine start failed"];
    return !_failsToStart;
}
- (void)stopWithCompletionHandler:(void (^)(NSError *))handler {
    for (TestHapticPlayer *player in _players) [player stopAtTime:0 error:nil];
    if (handler) handler(nil);
}
- (id<CHHapticAdvancedPatternPlayer>)createAdvancedPlayerWithPattern:(CHHapticPattern *)pattern error:(NSError **)error {
    assert(error != NULL);
    _pattern = pattern;
    TestHapticPlayer *player = [TestHapticPlayer new];
    [_players addObject:player];
    return (id<CHHapticAdvancedPatternPlayer>)player;
}
@end

@interface TestHaptics : NSObject
@property(nonatomic, strong) NSMutableArray<TestHapticEngine *> *engines;
- (CHHapticEngine *)createEngineWithLocality:(GCHapticsLocality)locality;
@end
@implementation TestHaptics
- (instancetype)init {
    if ((self = [super init])) _engines = [NSMutableArray array];
    return self;
}
- (CHHapticEngine *)createEngineWithLocality:(GCHapticsLocality)locality {
    assert([locality isEqualToString:GCHapticsLocalityDefault]);
    TestHapticEngine *engine = [TestHapticEngine new];
    [_engines addObject:engine];
    return (CHHapticEngine *)engine;
}
@end

@interface TestFeedbackController : NSObject
@property(nonatomic, strong) TestHaptics *haptics;
@end
@implementation TestFeedbackController
@end

static void test_haptics() {
    using namespace eka2l1::drivers::hwrm;
    TestFeedbackController *first = [TestFeedbackController new];
    first.haptics = [TestHaptics new];
    TestFeedbackController *second = [TestFeedbackController new];
    second.haptics = [TestHaptics new];
    set_vibration_suspended(false);
    set_controller_haptic_source((__bridge void *)first);
    auto vibrator = std::make_unique<vibrator_ios>();
    vibrator->vibrate(250, 80);
    assert(first.haptics.engines.count == 1);
    TestHapticEngine *engine = first.haptics.engines.lastObject;
    TestHapticPlayer *player = engine.players.lastObject;
    assert(player.starts == 1 && !player.loopEnabled);
    assert(std::abs(engine.pattern.duration - 0.25) < 0.001);
    vibrator->vibrate(0, -80);
    assert(player.stops > 0);
    player = engine.players.lastObject;
    assert(player.loopEnabled && player.loopEnd == 1 && player.starts == 1);
    vibrator->stop_vibrate();
    assert(player.stops > 0);

    vibrator->vibrate(100, 50);
    player = engine.players.lastObject;
    set_controller_haptic_source((__bridge void *)second);
    assert(player.stops > 0);
    vibrator->vibrate(100, 50);
    assert(second.haptics.engines.count == 1);
    engine = second.haptics.engines.lastObject;
    player = engine.players.lastObject;
    set_vibration_suspended(true);
    assert(player.stops > 0);
    vibrator->vibrate(100, 50);
    assert(engine.players.count == 1);
    set_vibration_suspended(false);
    vibrator->vibrate(100, 50);
    assert(second.haptics.engines.count == 2);
    engine = second.haptics.engines.lastObject;
    player = engine.players.lastObject;
    engine.failsToStart = YES;
    vibrator->vibrate(100, 50);
    assert(player.stops > 0 && engine.players.count == 1);
    engine.failsToStart = NO;
    vibrator->vibrate(100, 50);
    player = engine.players.lastObject;
    assert(player.starts == 1);

    engine.throwsOnStart = YES;
    vibrator->vibrate(100, 50);
    assert(player.stops > 0 && second.haptics.engines.count == 2);
    engine.throwsOnStart = NO;
    vibrator->vibrate(100, 50);
    assert(second.haptics.engines.count == 3);
    engine = second.haptics.engines.lastObject;
    player = engine.players.lastObject;
    assert(player.starts == 1);
    vibrator.reset();
    assert(player.stops > 0);
    set_controller_haptic_source(nullptr);
}

@interface HapticRecoveryTestDelegate : UIResponder <UIApplicationDelegate>
@end
@implementation HapticRecoveryTestDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        test_haptics();
        NSString *result = @"PASS: haptic routing, suspension, teardown, failed start retry and exception recovery\n";
        [result writeToFile:[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/result.txt"]
                 atomically:YES encoding:NSUTF8StringEncoding error:nil];
    });
    return YES;
}
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(HapticRecoveryTestDelegate.class));
    }
}
