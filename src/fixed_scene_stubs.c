/* Link-only scene stand-ins for the fixed-engine physical-layout build.
 * The playable ROM still links real scene modules in its transitional window. */
#include "v2_scene_arcade.h"
#include "v2_scene_gym.h"
#include "v2_scene_sidestreet.h"
#include "v2_scene_avenue.h"
#include "v2_scene_bar.h"
#include "v2_scene_casino.h"
#include "v2_scene_commentarium.h"
#include "v2_scene_mart.h"
#include "v2_scene_park.h"
#include "v2_scene_sewer.h"
#include "v2_scene_zoo.h"
#include "v2_game_state.h"
extern volatile unsigned char banked_scene_arg;
#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"
#include "v2_vram_queue.h"
#include "rng.h"

/* Recorded by each scene-entry stub: the first PRG bank it maps. music_select_song
 * briefly maps the music bank to cache a song, then restores this bank. */
extern unsigned char music_scene_bank;

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

/* Keep the complete scene-facing API physically present in the fixed-layout
 * validation image. The volatile gate is always clear in this stub-only ROM,
 * but its calls prevent the linker from discarding otherwise unused services. */
static volatile unsigned char fixed_api_probe;

static void keep_scene_api(void)
{
    mmc3_use_common_chr();
    mmc3_use_park_chr();
    mmc3_use_casino_cards_chr();
    mmc3_use_sewer_chr();
    mmc3_use_sidestreet_chr();
    mmc3_use_arcade_chr();
    mmc3_use_gym_chr();
    mmc3_use_mart_chr();
    /* The banked casino/commentarium seed their RNGs from the frame counter;
     * keep this fixed-engine entry point alive for them. */
    random_seed();
    /* Nothing left in the fixed engine itself calls rng_seed/rng_next now
     * that each scene's wandering-NPC logic keeps its own RngState locally
     * (see v2_scene_park.c and friends) instead of sharing v2_regulars.c --
     * without a keep-alive here the linker drops them as unreferenced and
     * every one of those scenes fails to link. */
    {
        RngState scratch;
        rng_seed(&scratch, 1);
        (void)rng_next(&scratch);
    }
    music_sfx_hurt();
    /* The bar scene's chugging challenge uses the pickup blip per gulp.
     * Keep it alive like the other scene-facing effects. */
    music_sfx_pickup();
    /* Generic UI/notification effects kept callable from any banked scene. */
    music_sfx_confirm();
    music_sfx_cancel();
    music_sfx_bell();
    music_select_song(MUSIC_SONG_AVENUE);
    music_play_jukebox_fanfare();
    clear_sprites();
    set_oam_sprite(0, 0, 0, 0, 0);
    hide_oam_sprite(0);
    oam_flush();
    reset_scroll();
    set_tile(0, 0, 0);
    fill_title_rect(0, 0, 0, 0, 0);
    fill_row(0, 0);
    set_attribute_block(0, 0, 0);
    draw_text(0, 0, "");
    draw_storefront_sign();
    draw_storefront_sign_at(0, 0, 2);
    draw_storefront_window(0, 0);
    draw_player_sprite(0, 0, 0, 0);
    hide_player_sprite();
    /* Park's, the Bar's, and the Gym's wandering-NPC logic each moved into
     * their own scene file (was v2_regulars.c) -- no keep-alive calls
     * belong here anymore, matching the casino note just below. */
    draw_taxi_sprite(0, 0);
    hide_taxi_sprite();
    draw_traffic_particles(0, 0);
    draw_crash_tableau(0, 0);
    set_traffic_crash_oam_base(48);
    dialogue_set_palette(3);
    (void)dialogue_get_palette();
    show_traffic_death();
    show_traffic_death_message("", "");
    /* The casino's broke game-over reuses the shared death plate. */
    show_broke_death();
    vram_queue_clear();
    run_inventory();
    /* v2_casino.c (blackjack/poker/slot game logic) lives in the casino
     * scene's own bank now, not the fixed engine -- see v2_game_state.c's
     * comment on why v2_game_reset() still inlines casino_reset()'s own
     * logic directly instead of calling it. No casino_* keep-alive calls
     * belong here anymore; the casino scene calls its own bank's copy
     * directly, no fixed-engine trampoline needed for any of it. */
}

/* The fixed-layout smoke target starts in the real Avenue after the title.
 * The production dispatcher remains transitional until the return routes are
 * banked as well. */
unsigned char run_neon_avenue(unsigned char arrival)
{
    if (fixed_api_probe) keep_scene_api();
    banked_scene_arg = arrival;
    music_scene_bank = 5;
    mmc3_select_prg_pair(5);
    return mmc3_call_mapped_scene();
}
unsigned char run_bar_interior(unsigned char from_casino) { banked_scene_arg = from_casino; music_scene_bank = 7; mmc3_select_prg_pair(7); return mmc3_call_mapped_scene(); }
unsigned char run_casino(void) { music_scene_bank = 13; mmc3_select_prg_pair(13); return mmc3_call_mapped_scene(); }
unsigned char run_mart_exterior(unsigned char from_mart) { banked_scene_arg = from_mart; music_scene_bank = 9; mmc3_select_prg_pair(9); return mmc3_call_mapped_scene(); }
void run_mart_interior(void) { banked_scene_arg = 2; music_scene_bank = 9; mmc3_select_prg_pair(9); mmc3_call_mapped_scene(); }
unsigned char run_park(void) { music_scene_bank = 3; mmc3_select_prg_pair(3); return mmc3_call_mapped_scene(); }
unsigned char run_sewer(void) { music_scene_bank = 11; mmc3_select_prg_pair(11); return mmc3_call_mapped_scene(); }
unsigned char run_zoo(void) { music_scene_bank = 15; mmc3_select_prg_pair(15); return mmc3_call_mapped_scene(); }
void run_commentarium(void) { music_scene_bank = 17; mmc3_select_prg_pair(17); mmc3_call_mapped_scene(); }
unsigned char run_arcade_exterior(void) { music_scene_bank = 21; mmc3_select_prg_pair(21); return mmc3_call_mapped_scene(); }
unsigned char run_sidestreet_exterior(void) { music_scene_bank = 23; mmc3_select_prg_pair(23); return mmc3_call_mapped_scene(); }
unsigned char run_gym_exterior(unsigned char spawn) { banked_scene_arg = spawn; music_scene_bank = 25; mmc3_select_prg_pair(25); return mmc3_call_mapped_scene(); }
void run_gym_interior(void) { banked_scene_arg = 2; music_scene_bank = 25; mmc3_select_prg_pair(25); mmc3_call_mapped_scene(); }
