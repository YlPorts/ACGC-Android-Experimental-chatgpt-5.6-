#include "jaudio_NES/cmdstack.h"
#include "jaudio_NES/playercall.h"
#include "jaudio_NES/rspsim.h"
#include "jaudio_NES/audiocommon.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
/* Platform hooks for these isolated tests; the engine uses its real OS layer. */
BOOL OSDisableInterrupts(void) { return TRUE; }
BOOL OSRestoreInterrupts(BOOL enabled) { return enabled; }
s32 Jac_RegisterPlayerCallback(PlayerCallBack callback,void* argument) {
    assert(callback && !argument); return 1;
}
void DCTouchRange(void* data,u32 size) { (void)data; (void)size; }
void DCStoreRange(void* data,u32 size) { (void)data; (void)size; }
void DCZeroRange(void* data,u32 size) { memset(data,0,size); }
static int sum;
static int add(int value) { sum+=value; return sum; }
int main(void) {
    JPorthead_ queue;
    int first[7]={0},second[7]={0};
    Jac_Porthead_Init(&queue);
    Set_Portcmd(first,add,7); Set_Portcmd(second,add,11);
    assert(Add_Portcmd(&queue,(u32*)first));
    assert(!Add_Portcmd(&queue,(u32*)first));
    assert(Add_Portcmd(&queue,(u32*)second));
    Jac_Portcmd_Proc_Once(&queue);
    assert(sum==18 && !queue._00 && !queue._04 && !first[3] && !second[3]);
    assert(Add_Portcmd(&queue,(u32*)first));
    Jac_Portcmd_Proc_Stay(&queue); Jac_Portcmd_Proc_Stay(&queue);
    assert(sum==32 && first[3]);
    Jac_Portcmd_Proc_Once(&queue); assert(sum==39 && !first[3]);

    u8* input=malloc(96); u8* output=calloc(1,96); assert(input && output);
    for(unsigned i=0;i<96;i++) input[i]=(u8)(i*13+3);
    Acmd commands[2];
    assert(sizeof(Acmd)==8);
    aLoadBuffer2(&commands[0],input,0x100,96);
    aSaveBuffer2(&commands[1],output,0x100,96);
    RspStart((u32*)commands,2);
    assert(!memcmp(input,output,96));
    free(input); free(output);
    return 0;
}
