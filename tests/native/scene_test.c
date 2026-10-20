#include "m_scene.h"
#include <assert.h>
#include <stddef.h>
static Actor_data actors[2];
static s16 controls[3],banks[2];
static Door_data_c doors[1];
static Scene_Word_u commands[] = {
    mSc_DATA_PLAYER(actors),
    mSc_DATA_CTRL_ACTORS(3,controls),
    mSc_DATA_ACTORS(2,actors),
    mSc_DATA_OBJ_BANK(2,banks),
    mSc_DATA_DOOR_DATA(1,doors),
    mSc_DATA_FIELDCT(2,3,0xa000,1,2),
    mSc_DATA_END(),
};
int main(void) {
    /* Compiled scene commands have several union views. Every pointer view
     * must read the native address initialized through misc.param3. */
    assert(offsetof(Scene_Word_Data_Misc_c,param3)==offsetof(Scene_Word_Data_Actor_c,data_p));
    assert(commands[0].actor.data_p==actors && commands[0].actor.num_actors==1);
    assert(commands[1].control_actor.ctrl_actor_profile_p==controls);
    assert(commands[2].actor.data_p==actors && commands[2].actor.num_actors==2);
    assert(commands[3].object_bank.banks_p==banks);
    assert(commands[4].door_data.door_data_p==doors);
    assert(commands[5].misc.param3==0xa0000102u);
    assert(commands[5].field_ct.item_type==2 && commands[5].field_ct.bg_num==3);
    assert(commands[6].misc.type==mSc_SCENE_DATA_TYPE_END);
    return 0;
}
