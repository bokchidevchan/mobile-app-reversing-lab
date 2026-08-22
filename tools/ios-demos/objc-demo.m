#import <Foundation/Foundation.h>
@interface CryptoManager : NSObject
- (NSString *)decryptWithKey:(NSString *)key;
@end
@implementation CryptoManager
- (NSString *)decryptWithKey:(NSString *)key { return [@"plain-" stringByAppendingString:key]; }
@end
int main(){ @autoreleasepool { CryptoManager *c=[CryptoManager new]; NSLog(@"%@",[c decryptWithKey:@"K"]); } return 0; }
