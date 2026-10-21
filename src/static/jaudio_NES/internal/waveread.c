#include "jaudio_NES/waveread.h"

#include "jaudio_NES/connect.h"
#include "jaudio_NES/heapctrl.h"
#include "jaudio_NES/bx.h"

#define WAVEARC_SIZE   (0x100)
#define WAVEGROUP_SIZE (0x100)

static WaveArchiveBank_* wavearc[WAVEARC_SIZE];
static CtrlGroup_* wavegroup[WAVEGROUP_SIZE];
CtrlGroup_* CGRP_ARRAY[16];
#ifdef PC_NATIVE_64
#include "pc_bx.h"
static PcBx* pc_wavegroups[WAVEGROUP_SIZE];
static PcBx* pc_test_wsys;
static u32 pc_wave_u32(const u8* p) {
    return ((u32)p[0]<<24) | ((u32)p[1]<<16) | ((u32)p[2]<<8) | p[3];
}
#endif

/*
 * --INFO--
 * Address:	8000C200
 * Size:	000038
 */

#ifndef PC_NATIVE_64
static void PTconvert(void** pointer, u32 base_address)
{
	if (*pointer == NULL) {
		*pointer = NULL;
		return;
	}
	if (*pointer >= (void*)base_address || *pointer == NULL) {
		return;
	}
	*pointer = *(char**)pointer + base_address;
}

/*
 * --INFO--
 * Address:	8000C240
 * Size:	0002A0
 */
#endif
CtrlGroup_* Wave_Test(u8* data)
{
#ifdef PC_NATIVE_64
    PcBx* decoded=pc_bx_decode(data,pc_wave_u32(data+4),1);
    if (!decoded) return NULL;
    pc_bx_free(pc_test_wsys); pc_test_wsys=decoded;
    CGRP_ARRAY[0]=pc_bx_wsys(decoded)->ctrlGroup;
    return CGRP_ARRAY[0];
#else
    u32 base_addr = (u32)data;
	CtrlGroup_* group;
	SCNE_* scene;
	Ctrl_* cst;
	Ctrl_* cdf;
	Ctrl_* cex;
	u32 i;
    u32 j;
	WaveArchiveBank_* arcBank;
	WaveArchive_* arc;

	PTconvert((void**)&((Wsys_*)data)->waveArcBank, base_addr);
	PTconvert((void**)&((Wsys_*)data)->ctrlGroup, base_addr);
	arcBank       = *(WaveArchiveBank_**)(data + 0x10);
	group         = *(CtrlGroup_**)(data + 0x14);
	CGRP_ARRAY[0] = group;

	if (arcBank->magic != 'WINF') {
		return NULL;
	}
	if (group->magic != 'WBCT') {
		return NULL;
	}

	for (i = 0; i < arcBank->count; i++) {
		PTconvert((void**)&arcBank->waveGroups[i], base_addr);
		arc     = arcBank->waveGroups[i];
		Jac_InitHeap(&arc->heap);
		arc->heap.startAddress = 0;

		for (j = 0; j < arc->waveCount; j++) {
			PTconvert((void**)&arc->waves[j], base_addr);
		}
	}

	for (i = 0; i < group->count; i++) {
		PTconvert((void**)&group->scenes[i], base_addr);
		scene = group->scenes[i];
		PTconvert((void**)&scene->cdf, base_addr);
		PTconvert((void**)&scene->cex, base_addr);
		PTconvert((void**)&scene->cst, base_addr);

		cdf = scene->cdf;
		if (cdf && cdf->magic == 'C-DF') {
			for (j = 0; j < cdf->count; j++) {
				PTconvert((void**)&cdf->waveIDs[j], base_addr);
				Jac_InitHeap(&cdf->waveIDs[j]->heap);
				cdf->waveIDs[j]->heap.startAddress = 0;
			}
		}

		cex = scene->cex;
		if (cex && cex->magic == 'C-EX') {
			for (j = 0; j < cex->count; j++) {
				PTconvert((void**)&cex->waveIDs[j], base_addr);
				Jac_InitHeap(&cex->waveIDs[j]->heap);
				cex->waveIDs[j]->heap.startAddress = 0;
			}
		}

		cst = scene->cst;
		if (cst && cst->magic == 'C-ST') {
			for (j = 0; j < cst->count; j++) {
				PTconvert((void**)&cst->waveIDs[j], base_addr);
				Jac_InitHeap(&cst->waveIDs[j]->heap);
				cst->waveIDs[j]->heap.startAddress = 0;
			}
		}
	}
	return CGRP_ARRAY[0];
#endif
}

/*
 * --INFO--
 * Address:	........
 * Size:	000030
 */
void GetSound_Test(u32 id)
{
	// UNUSED FUNCTION
}

/*
 * --INFO--
 * Address:	8000C4E0
 * Size:	000084
 */
BOOL Wavegroup_Regist(void* wsysData, u32 id)
{
    if (id >= WAVEGROUP_SIZE) return FALSE;
#ifdef PC_NATIVE_64
    PcBx* decoded=pc_bx_decode(wsysData,pc_wave_u32((u8*)wsysData+4),1);
    if (!decoded) return FALSE;
    pc_bx_free(pc_wavegroups[id]); pc_wavegroups[id]=decoded;
    Wsys_* wsys=pc_bx_wsys(decoded);
    wavegroup[id]=wsys->ctrlGroup;
#else
    Wsys_* wsys = (Wsys_*)wsysData;
    wavegroup[id] = Wave_Test((u8*)wsys);
#endif
    Jac_WsConnectTableSet(wsys->globalID, id);
    wavearc[id] = wsys->waveArcBank;

	if (wavegroup[id] == NULL) {
		return FALSE;
	}
	wavegroup[id]->_04 = 0;
	return TRUE;
}

/*
 * --INFO--
 * Address:	8000C580
 * Size:	00002C
 */
void Wavegroup_Init()
{
#ifdef PC_NATIVE_64
    pc_bx_free(pc_test_wsys); pc_test_wsys=NULL;
#endif
	for (int i = 0; i < WAVEGROUP_SIZE; ++i) {
#ifdef PC_NATIVE_64
        pc_bx_free(pc_wavegroups[i]); pc_wavegroups[i]=NULL;
#endif
        wavearc[i]=NULL;
        wavegroup[i] = NULL;
	}
}

/*
 * --INFO--
 * Address:	8000C5C0
 * Size:	000064
 */
CtrlGroup_* WaveidToWavegroup(u32 param_1, u32 param_2)
{
	u16 virtID = param_1 >> 16;
	u16 index;
	u16* REF_virtID = &virtID;

	if (virtID == 0xFFFF) {
		index = param_2;
	} else {
		index = Jac_WsVirtualToPhysical(virtID);
	}

	return index >= WAVEGROUP_SIZE ? NULL : wavegroup[index];
}

/*
 * --INFO--
 * Address:	8000C640
 * Size:	00008C
 */
static BOOL __WaveScene_Set(u32 waveIndex, u32 ctrlIndex, BOOL doSet)
{
	u32* REF_param_1;
	u32* REF_param_2;

	CtrlGroup_* group;

	REF_param_1 = &waveIndex;
	if (waveIndex >= WAVEGROUP_SIZE) {
		return FALSE;
	}
	if (!(group = wavegroup[waveIndex])) {
		return FALSE;
	}
	REF_param_2 = &ctrlIndex;
	if (ctrlIndex >= group->count) {
		return FALSE;
	}
	return Jac_SceneSet(wavearc[waveIndex], group, ctrlIndex, doSet);
}

/*
 * --INFO--
 * Address:	8000C6E0
 * Size:	000024
 */
BOOL WaveScene_Set(u32 waveIndex, u32 ctrlIndex)
{
	return __WaveScene_Set(waveIndex, ctrlIndex, TRUE);
}

/*
 * --INFO--
 * Address:	8000C720
 * Size:	000024
 */
BOOL WaveScene_Load(u32 waveIndex, u32 ctrlIndex)
{
	return __WaveScene_Set(waveIndex, ctrlIndex, FALSE);
}

/*
 * --INFO--
 * Address:	8000C760
 * Size:	000074
 */
static void __WaveScene_Close(u32 waveIndex, u32 ctrlIndex, BOOL param_3)
{
	u32* REF_param_1;
	u32* REF_param_2;

	CtrlGroup_* group;

	REF_param_1 = &waveIndex;
	if (waveIndex >= WAVEGROUP_SIZE) {
		return;
	}
	if (group = wavegroup[waveIndex]) {
		REF_param_2 = &ctrlIndex;
		if (ctrlIndex < group->count) {
			Jac_SceneClose(wavearc[waveIndex], group, ctrlIndex, param_3);
		}
	}
}

/*
 * --INFO--
 * Address:	8000C7E0
 * Size:	000024
 */
void WaveScene_Close(u32 waveIndex, u32 ctrlIndex)
{
	__WaveScene_Close(waveIndex, ctrlIndex, TRUE);
}

/*
 * --INFO--
 * Address:	8000C820
 * Size:	000024
 */
void WaveScene_Erase(u32 waveIndex, u32 ctrlIndex)
{
	__WaveScene_Close(waveIndex, ctrlIndex, FALSE);
}
