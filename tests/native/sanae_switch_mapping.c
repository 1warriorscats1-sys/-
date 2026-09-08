/* Synthetic libnx inputs, exercising the exact shipped mapping include. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t u64;
#define BIT(n) (UINT64_C(1) << (n))
enum {
 HidNpadButton_A=BIT(0), HidNpadButton_B=BIT(1),
 HidNpadButton_X=BIT(2), HidNpadButton_Y=BIT(3),
 HidNpadButton_StickL=BIT(4), HidNpadButton_StickR=BIT(5),
 HidNpadButton_L=BIT(6), HidNpadButton_R=BIT(7),
 HidNpadButton_ZL=BIT(8), HidNpadButton_ZR=BIT(9),
 HidNpadButton_Plus=BIT(10), HidNpadButton_Minus=BIT(11),
 HidNpadButton_Left=BIT(12), HidNpadButton_Up=BIT(13),
 HidNpadButton_Right=BIT(14), HidNpadButton_Down=BIT(15),
 HidNpadButton_StickLLeft=BIT(16), HidNpadButton_StickLUp=BIT(17),
 HidNpadButton_StickLRight=BIT(18), HidNpadButton_StickLDown=BIT(19),
 HidNpadButton_StickRLeft=BIT(20), HidNpadButton_StickRUp=BIT(21),
 HidNpadButton_StickRRight=BIT(22), HidNpadButton_StickRDown=BIT(23),
 HidNpadButton_AnyLeft=BIT(12)|BIT(16)|BIT(20),
 HidNpadButton_AnyUp=BIT(13)|BIT(17)|BIT(21),
 HidNpadButton_AnyRight=BIT(14)|BIT(18)|BIT(22),
 HidNpadButton_AnyDown=BIT(15)|BIT(19)|BIT(23)
};
typedef struct { int x,y; } HidAnalogStickState;
typedef struct { HidAnalogStickState stick[2]; } PadState;
typedef struct { bool buttonDown[17]; float buttonValue[17],axisValue[4]; } GamepadSlot;
static HidAnalogStickState padGetStickPos(PadState *pad,int stick) {
 assert(stick==0); /* No right-stick reads allowed. */
 return pad->stick[stick];
}
#include "sanae_switch_mapping.inc"
int main(void) {
 PadState pad={0}; GamepadSlot slot;
 const u64 keys[]={HidNpadButton_A,HidNpadButton_B,HidNpadButton_Y,HidNpadButton_X,
 HidNpadButton_L,HidNpadButton_R,HidNpadButton_Minus,HidNpadButton_Plus,HidNpadButton_StickL,
 HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_Left,HidNpadButton_Right,
 HidNpadButton_StickLUp,HidNpadButton_StickLDown,HidNpadButton_StickLLeft,HidNpadButton_StickLRight};
 const int targets[]={1,0,2,3,4,5,8,9,10,12,13,14,15,12,13,14,15};
 for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i) {
  memset(&slot,0,sizeof(slot));mapLibnxToGml(&slot,&pad,keys[i]);
  for(int j=0;j<17;++j) assert(slot.buttonDown[j]==(j==targets[i]));
 }
 const u64 right=HidNpadButton_StickR|HidNpadButton_StickRLeft|HidNpadButton_StickRRight|HidNpadButton_StickRUp|HidNpadButton_StickRDown;
 pad.stick[1]=(HidAnalogStickState){32767,-32767};
 for(int bit=0;bit<24;++bit) if(right & BIT(bit)) {
  memset(&slot,0,sizeof(slot));slot.axisValue[2]=1;slot.axisValue[3]=-1;
  mapLibnxToGml(&slot,&pad,BIT(bit));
  for(int j=0;j<17;++j) assert(!slot.buttonDown[j]);
  assert(slot.axisValue[2]==0 && slot.axisValue[3]==0);
 }
 pad.stick[0]=(HidAnalogStickState){32767,-32767};
 memset(&slot,0,sizeof(slot));mapLibnxToGml(&slot,&pad,right|HidNpadButton_A|HidNpadButton_Up|HidNpadButton_ZL|HidNpadButton_ZR);
 assert(slot.buttonDown[1] && slot.buttonDown[12] && !slot.buttonDown[11]);
 assert(slot.axisValue[0]==1 && slot.axisValue[1]==1 && slot.axisValue[2]==0 && slot.axisValue[3]==0);
 assert(slot.buttonValue[6]==1 && slot.buttonValue[7]==1);
 puts("Switch mapping: A/B swapped, X/Y retained, right stick/R3 inert, left/D-pad/shoulders retained.");
 return 0;
}
