#import "SpotView.h"

@implementation SpotView

//____________________________________________________________________________________________________
- (id) initWithFrame : (CGRect) frame
{
   if (self = [super initWithFrame : frame]) {
      self.alpha = 0.f;
      self.multipleTouchEnabled = NO;
      self.backgroundColor = [UIColor orangeColor];
   }

   return self;
}

@end
