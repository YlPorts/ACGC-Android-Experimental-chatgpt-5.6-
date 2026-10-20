#include "pc_language.h"
#include "jsyswrap.h"
#include "jaudio_NES/audioheaders.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* The legacy audio header names a type `link`. This test only needs the
 * cwd API, so keep the unrelated POSIX link() declaration out of that name. */
#define link acgc_test_posix_link
#include <unistd.h>
#undef link
static void write_file(const char* path,const void* bytes,size_t size) {
    FILE* f=fopen(path,"wb"); assert(f);
    assert(fwrite(bytes,1,size,f)==size && fclose(f)==0);
}
int main(void) {
    char original[4096],directory[]="/tmp/acgc-multi5-XXXXXX",path[512];
    assert(getcwd(original,sizeof(original)) && mkdtemp(directory) && chdir(directory)==0);
    assert(mkdir("languages",0700)==0);
    const char* codes[]={"es","fr","de","it"};
    u8 resource[32],graphic[32],pair_a[32],pair_b[32],replacement[32];
    memset(graphic,0x11,sizeof(graphic)); memset(pair_a,0x22,sizeof(pair_a));
    memset(pair_b,0x33,sizeof(pair_b)); memset(replacement,0x77,sizeof(replacement));
    u8 audio_original[sizeof(ArcHeader)+249*sizeof(ArcEntry)];
    memcpy(audio_original,&AudioseqHeaderStart,sizeof(audio_original));
    pc_language_register_graphic("test.bin",graphic,sizeof(graphic));
    pc_language_register_graphic_pair("pair-a.bin",pair_a,32,"pair-b.bin",pair_b,32);
    for(unsigned i=0;i<4;i++) {
        snprintf(path,sizeof(path),"languages/%s",codes[i]); assert(mkdir(path,0700)==0);
        const char* folders[]={"aram","graphics","audio"};
        for(unsigned f=0;f<3;f++) { snprintf(path,sizeof(path),"languages/%s/%s",codes[i],folders[f]); assert(mkdir(path,0700)==0); }
        for(unsigned j=0;j<sizeof(resource);j++) resource[j]=(u8)(i*32+j);
        snprintf(path,sizeof(path),"languages/%s/aram/npc_name_str_table.bin",codes[i]); write_file(path,resource,32);
        /* An incomplete pair must keep the original resource address. */
        snprintf(path,sizeof(path),"languages/%s/aram/mail_data.bin",codes[i]); write_file(path,resource,32);
        snprintf(path,sizeof(path),"languages/%s/ui_strings.ini",codes[i]);
        const char ini[]="test.title=Multi5 test\n"; write_file(path,ini,sizeof(ini)-1);
        snprintf(path,sizeof(path),"languages/%s/graphics/test.bin",codes[i]); write_file(path,replacement,32);
        snprintf(path,sizeof(path),"languages/%s/graphics/pair-a.bin",codes[i]); write_file(path,replacement,32);
        /* Merely finding regional audio files must never enable PAL audio. */
        snprintf(path,sizeof(path),"languages/%s/audio/audiorom.img",codes[i]); write_file(path,resource,32);
        snprintf(path,sizeof(path),"languages/%s/audio/audio_headers.bin",codes[i]); write_file(path,resource,32);
        pc_language_init(codes[i]);
        assert(!strcmp(pc_language_code(),codes[i]) && pc_language_is_external());
        assert(!pc_language_audio_override_enabled());
        assert(!memcmp(audio_original,&AudioseqHeaderStart,sizeof(audio_original)));
        assert(!strcmp(pc_language_ui_lookup("test.title","fallback"),"Multi5 test"));
        assert(!strcmp(pc_language_ui_lookup("absent.key","fallback"),"fallback"));
        u32 address=pc_language_override_address(RESOURCE_NPC_NAME_STR_TABLE,0x12345678);
        assert(address>=0xe0000000u && address<0xf0000000u);
        u8 out[64]; assert(pc_language_read_aram(address,out,64));
        assert(!memcmp(out,resource,32));
        for(unsigned j=32;j<64;j++) assert(out[j]==0);
        assert(!pc_language_read_aram(UINT32_MAX-3,out,64));
        assert(pc_language_override_address(RESOURCE_MAIL,0x12345678)==0x12345678);
        assert(!memcmp(graphic,replacement,32) && pair_a[0]==0x22 && pair_b[0]==0x33);
        pc_language_init("en");
        assert(!pc_language_is_external() && graphic[0]==0x11 && !pc_language_audio_override_enabled());
    }
    write_file("languages/es/audio/enable_pal_audio.flag","",0);
    pc_language_init("es"); /* Explicit opt-in still rejects the malformed pair. */
    assert(!pc_language_audio_override_enabled());
    assert(!memcmp(audio_original,&AudioseqHeaderStart,sizeof(audio_original)));
    pc_language_audio_fallback_to_usa();
    pc_language_shutdown();
    assert(chdir(original)==0);
    puts("Multi5: four synthetic packs, virtual ARAM, UI/graphics fallback and audio SafeBoot passed");
    return 0;
}
