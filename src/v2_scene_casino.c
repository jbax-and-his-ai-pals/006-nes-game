#include <nes.h>

#include "font.h"
#include "mmc3.h"
#include "music.h"
#include "scene_casino_data.h"
#include "v2_casino.h"
#include "v2_game_state.h"
#include "v2_input.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_casino.h"
#include "v2_scene_ui.h"
#include "v2_vram_queue.h"

#define FONT_TILE_BASE 8
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53
/* The card/casino games' own dedicated CHR bank (see
 * mmc3_use_casino_cards_chr() and casino_cards.s) -- font.s is linked into
 * it at the same index 8 offset as the common bank, so FONT_TILE_BASE/
 * TILE_BORDER_H/V above stay meaningless there; these are the new tiles it
 * adds. Only used inside show_casino_game_screen() and its three callers
 * (blackjack, poker, slots) -- nothing else in this file runs while that
 * bank is selected. */
#define CARD_BORDER_H 51
#define CARD_BORDER_V 52
#define CARD_SUIT_SPADE 53
#define CARD_SUIT_CLUB 54
#define CARD_SUIT_HEART 55
#define CARD_SUIT_DIAMOND 56
#define CARD_BACK 57
#define CARD_SLOT_SEVEN 58
#define CARD_SLOT_CHERRY 59
#define CARD_WIDTH 4
#define CARD_HEIGHT 4
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_SHELF 238
#define BAR_TILE_JUKEBOX_TOP 240
#define BAR_TILE_JUKEBOX_BOT 241
#define BAR_TILE_STOOL 243
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245

#define casino_credits game_state.casino.credits
#define casino_bet game_state.casino.bet
#define casino_reel game_state.casino.reel

/* Shared between two or more games below, each of which used to carry its
 * own copy of the identical literal -- this scene's own PRG bank is tight
 * enough that the duplication was a real cost, not just tidiness. */
static const char poker_title[] = "POKER";
static const char you_win_text[] = "YOU WIN +";
static const char lr_bet_text[] = "L R: BET";

/* Defined below (shared header/clear-screen helper for all three games);
 * forward-declared so show_slots_screen() can call it despite coming
 * first in the file. */
static void show_casino_game_screen(const char *title);
/* Defined below (shared payout-table row renderer for poker). */
static void draw_payout_row(unsigned char row, const char *name, unsigned char multiplier);
/* Centers whichever symbol is showing within a slot window's 7-tile
 * interior (see SLOTS_WINDOW_SPACING). SEVEN and CHERRY are single
 * graphic tiles landing on the window's middle column; BAR has no
 * dedicated graphic (block capitals already read fine as a "graphic")
 * and spans the same three middle columns instead. Every symbol's
 * footprint is a subset of those three columns, so redrawing them
 * unconditionally always fully overwrites whatever was showing there
 * before -- no separate clear pass needed. Shared by draw_slot_symbol()
 * and queue_slot_symbol() below, which used to each carry their own copy
 * of this switch -- one for immediate set_tile(), one for queued
 * vram_queue_tile() -- this scene's own PRG bank is tight enough that the
 * duplicated logic was a real cost. */
static void slot_symbol_tiles(unsigned char symbol, unsigned char *tiles)
{
    switch (symbol) {
        case CASINO_SYMBOL_SEVEN:
            tiles[0] = FONT_TILE_BASE; tiles[1] = CARD_SLOT_SEVEN; tiles[2] = FONT_TILE_BASE;
            break;
        case CASINO_SYMBOL_CHERRY:
            tiles[0] = FONT_TILE_BASE; tiles[1] = CARD_SLOT_CHERRY; tiles[2] = FONT_TILE_BASE;
            break;
        default:
            tiles[0] = (unsigned char)(FONT_TILE_BASE + font_glyph_index('B'));
            tiles[1] = (unsigned char)(FONT_TILE_BASE + font_glyph_index('A'));
            tiles[2] = (unsigned char)(FONT_TILE_BASE + font_glyph_index('R'));
            break;
    }
}

static void draw_slot_symbol(unsigned char row, unsigned char left_col, unsigned char symbol)
{
    unsigned char mid = (unsigned char)(left_col + 4);
    unsigned char tiles[3];

    slot_symbol_tiles(symbol, tiles);
    set_tile(row, (unsigned char)(mid - 1), tiles[0]);
    set_tile(row, mid, tiles[1]);
    set_tile(row, (unsigned char)(mid + 1), tiles[2]);
}

/* Dynamic casino content fits in one 32-tile NMI batch. Initial scene and
 * panel construction still use forced blank; only these frequent redraws need
 * to stay visible while play continues. */
static void queue_slot_symbol(unsigned char row, unsigned char left_col, unsigned char symbol)
{
    unsigned char mid = (unsigned char)(left_col + 4);
    unsigned char tiles[3];

    slot_symbol_tiles(symbol, tiles);
    (void)vram_queue_tile(row, (unsigned char)(mid - 1), tiles[0]);
    (void)vram_queue_tile(row, mid, tiles[1]);
    (void)vram_queue_tile(row, (unsigned char)(mid + 1), tiles[2]);
}

/* Always exactly 2 digits -- for values that genuinely never reach 100:
 * bet (1-10), and blackjack's hand totals/payouts (a natural at max bet
 * is 25, the highest blackjack ever pays). Credits and poker payouts can
 * both reach 255 -- see draw_digits3()/queue_digits3() below for those,
 * this would silently show only their last two digits. */
static void draw_digits2(unsigned char row, unsigned char col, unsigned char value)
{
    char buf[3];
    buf[0] = (char)('0' + (value / 10) % 10);
    buf[1] = (char)('0' + value % 10);
    buf[2] = 0;
    draw_text(row, col, buf);
}

static void queue_digits2(unsigned char row, unsigned char col, unsigned char value)
{
    (void)vram_queue_tile(row, col,
                          (unsigned char)(FONT_TILE_BASE + font_glyph_index((char)('0' + (value / 10) % 10))));
    (void)vram_queue_tile(row, (unsigned char)(col + 1),
                          (unsigned char)(FONT_TILE_BASE + font_glyph_index((char)('0' + value % 10))));
}

/* Up to 3 digits, space-padded on the left to a fixed 3-tile field (" 5",
 * not "5", so a value that used to need 3 digits and now needs fewer
 * doesn't leave a stale leading digit behind when this redraws in place --
 * matters for queue_digits3() below, which updates a live field rather
 * than redrawing after a full clear). For credits (max 255) and poker
 * payouts (also up to 255, see casino_poker_payout()'s clamp comment). */
static void draw_digits3(unsigned char row, unsigned char col, unsigned char value)
{
    char buf[4];
    buf[0] = value >= 100 ? (char)('0' + value / 100) : ' ';
    buf[1] = value >= 10 ? (char)('0' + (value / 10) % 10) : ' ';
    buf[2] = (char)('0' + value % 10);
    buf[3] = 0;
    draw_text(row, col, buf);
}

static void queue_digits3(unsigned char row, unsigned char col, unsigned char value)
{
    char d0 = value >= 100 ? (char)('0' + value / 100) : ' ';
    char d1 = value >= 10 ? (char)('0' + (value / 10) % 10) : ' ';
    char d2 = (char)('0' + value % 10);

    (void)vram_queue_tile(row, col, (unsigned char)(FONT_TILE_BASE + font_glyph_index(d0)));
    (void)vram_queue_tile(row, (unsigned char)(col + 1), (unsigned char)(FONT_TILE_BASE + font_glyph_index(d1)));
    (void)vram_queue_tile(row, (unsigned char)(col + 2), (unsigned char)(FONT_TILE_BASE + font_glyph_index(d2)));
}

/* Slots' own full-screen layout (see show_slots_screen()/run_slots_game()):
 * a payout table (see draw_slots_payout_table()) at the top, three
 * windows spanning rows SLOTS_WINDOW_TOP-SLOTS_WINDOW_BOTTOM below it,
 * each SLOTS_WINDOW_SPACING tiles apart, the lever to their right, and
 * the per-pull result (see show_slots_result()) below that. Credits/bet
 * are the same header show_casino_game_screen() already draws at row 5 --
 * these only need to keep that in sync as they change during play, not
 * draw a second copy. */
#define SLOTS_PAYOUT_TABLE_TOP 7
#define SLOTS_WINDOW_TOP 13
#define SLOTS_SYMBOL_ROW 15
#define SLOTS_WINDOW_BOTTOM 17
#define SLOTS_WINDOW_SPACING 9
#define SLOTS_LEVER_COL 29
#define SLOTS_LEVER_TOP_ROW 13
#define SLOTS_RESULT_ROW 19

static void draw_casino_hud(void)
{
    draw_slot_symbol(SLOTS_SYMBOL_ROW, 0, casino_reel[0]);
    draw_slot_symbol(SLOTS_SYMBOL_ROW, (unsigned char)(1 * SLOTS_WINDOW_SPACING), casino_reel[1]);
    draw_slot_symbol(SLOTS_SYMBOL_ROW, (unsigned char)(2 * SLOTS_WINDOW_SPACING), casino_reel[2]);
}

/* Multipliers here are written out matching casino_payout()'s own table
 * (see v2_casino.c) exactly -- same by-hand-sync discipline as
 * draw_poker_payout_table(), no shared source of truth between game logic
 * and this display. Symbol names come from casino_slot_symbols[] itself
 * rather than being retyped, so at least those two can't drift apart. */
static void draw_slots_payout_table(void)
{
    static const unsigned char multiplier[CASINO_SYMBOL_COUNT] = {10, 7, 5};
    unsigned char i;

    for (i = 0; i < CASINO_SYMBOL_COUNT; ++i) {
        draw_payout_row((unsigned char)(SLOTS_PAYOUT_TABLE_TOP + i), casino_slot_symbols[i], multiplier[i]);
    }
    /* BAR is wild (see casino_is_winner() in v2_casino.c) -- it stands in
     * for whichever other symbol the remaining reels show, so e.g. 7-7-
     * BAR still wins at 7's rate. Spelled out here since nothing else on
     * screen would tell the player a mismatched pull can still be a win. */
    draw_text((unsigned char)(SLOTS_PAYOUT_TABLE_TOP + CASINO_SYMBOL_COUNT), 2, "BAR WILD");
}

/* Lengths of casino_slot_symbols[]'s own three strings ("7"/"BAR"/
 * "CHERRY"), index-matched -- a runtime strlen loop costs more code than
 * this 3-byte table in a bank this tight. */
static const unsigned char slot_symbol_len[CASINO_SYMBOL_COUNT] = {1, 3, 6};

/* The result of the most recent pull -- spells out all three reels plus
 * the outcome ("7 7 BAR WIN +10") rather than a bare "WIN"/"NO WIN",
 * which didn't say what actually lined up or how much it paid, and read
 * as unclear once BAR's wild substitution meant a win no longer has to be
 * three matching symbols. Worst case ("CHERRY CHERRY CHERRY WIN +NNN",
 * the longest name three times over) is well under a row's 32 columns.
 *
 * Unlike the reel/credits/bet HUD, this doesn't go through the NMI queue:
 * it only changes once per pull (when the spin settles, see
 * run_slots_game()), not continuously during one, and adding a message
 * this long to the same queued batch that already updates the reels/
 * credits/bet/lever would overflow the queue's fixed 32-entry capacity.
 * A one-shot forced-blank write, the same as every other non-continuous
 * update in this file, is simpler and safe regardless of how long it
 * takes. */
static void show_slots_result(void)
{
    unsigned char payout = casino_payout(&game_state.casino);
    unsigned char col = 0;
    unsigned char i;

    PPU.mask = 0;
    fill_title_rect(SLOTS_RESULT_ROW, 0, 32, 1, FONT_TILE_BASE);
    for (i = 0; i < CASINO_REEL_COUNT; ++i) {
        draw_text(SLOTS_RESULT_ROW, col, casino_slot_symbols[casino_reel[i]]);
        col = (unsigned char)(col + slot_symbol_len[casino_reel[i]] + 1);
    }
    if (payout) {
        draw_text(SLOTS_RESULT_ROW, col, "WIN +");
        draw_digits3(SLOTS_RESULT_ROW, (unsigned char)(col + 5), payout);
    } else {
        draw_text(SLOTS_RESULT_ROW, col, "NO WIN");
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void draw_slots_windows(void)
{
    unsigned char i, left_col;

    for (i = 0; i < 3; ++i) {
        left_col = (unsigned char)(i * SLOTS_WINDOW_SPACING);
        fill_title_rect(SLOTS_WINDOW_TOP, (unsigned char)(left_col + 1), 7, 1, CARD_BORDER_H);
        fill_title_rect(SLOTS_WINDOW_BOTTOM, (unsigned char)(left_col + 1), 7, 1, CARD_BORDER_H);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 1), left_col, CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 2), left_col, CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 3), left_col, CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 1), (unsigned char)(left_col + 8), CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 2), (unsigned char)(left_col + 8), CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_WINDOW_TOP + 3), (unsigned char)(left_col + 8), CARD_BORDER_V);
    }
}

/* The upright lever is drawn once with the panel. During a pull, its knob
 * (borrowed from the card bank's spade tile -- a small round shape reads
 * fine as a lever knob, and there's no dedicated art for one) drops to the
 * bottom of its two-tile travel; four queued tile updates are enough to
 * change poses without disturbing the reel batch. */
static void draw_slots_lever(unsigned char pulled)
{
    set_tile(SLOTS_LEVER_TOP_ROW, SLOTS_LEVER_COL, CARD_BORDER_V);
    set_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 1), SLOTS_LEVER_COL, CARD_BORDER_V);
    if (pulled) {
        set_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 2), SLOTS_LEVER_COL, CARD_BORDER_V);
        set_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 3), SLOTS_LEVER_COL, CARD_SUIT_SPADE);
    } else {
        set_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 2), SLOTS_LEVER_COL, CARD_SUIT_SPADE);
        set_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 3), SLOTS_LEVER_COL, FONT_TILE_BASE);
    }
}

static void queue_slots_lever(unsigned char pulled)
{
    if (pulled) {
        (void)vram_queue_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 2), SLOTS_LEVER_COL, CARD_BORDER_V);
        (void)vram_queue_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 3), SLOTS_LEVER_COL, CARD_SUIT_SPADE);
    } else {
        (void)vram_queue_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 2), SLOTS_LEVER_COL, CARD_SUIT_SPADE);
        (void)vram_queue_tile((unsigned char)(SLOTS_LEVER_TOP_ROW + 3), SLOTS_LEVER_COL, FONT_TILE_BASE);
    }
}

static void queue_casino_hud(const unsigned char *display_reel, unsigned char lever_pulled)
{
    queue_slot_symbol(SLOTS_SYMBOL_ROW, 0, display_reel[0]);
    queue_slot_symbol(SLOTS_SYMBOL_ROW, (unsigned char)(1 * SLOTS_WINDOW_SPACING), display_reel[1]);
    queue_slot_symbol(SLOTS_SYMBOL_ROW, (unsigned char)(2 * SLOTS_WINDOW_SPACING), display_reel[2]);
    queue_digits3(5, 12, casino_credits);
    queue_digits2(5, 23, casino_bet);
    queue_slots_lever(lever_pulled);
}

/* The actual "Morrowind resolution"-free gamble: spend the bet, roll
 * three independent symbols, and pay out on a three-of-a-kind. Simple
 * on purpose for this first rebuilt pass -- weighted odds, a proper
 * payout table per symbol, and queued reel animation are exactly what
 * REBUILD_ROADMAP.md calls out as later refinement, not this slice. */
static void show_casino(void)
{
    static const unsigned char casino_palette[32] = {
        /* BG pal0: the room's warm wood floor/walls (unchanged). BG
         * pal1: the blackjack table's own rail/felt colors (see the
         * attribute loop below) -- index 1 (0x07) is the same "warm
         * wood" rail tone this array already uses for the floor/walls,
         * index 2 (0x1A) is the felt green show_casino_game_screen()
         * uses for blackjack/poker/slots. Applied only to the table's
         * own two attribute cells, not the floor. */
        0x07, 0x17, 0x08, 0x28, 0x0F, 0x07, 0x1A, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x07, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = casino_palette[i];

    /* Same wood-floor/wall tiles as the bar -- a shady backroom, not a
     * different building, and it keeps this scene inside the already-
     * exhausted 256-tile CHR budget (see TOKEN_TILE's comment) with no
     * new background art at all. South wall is 3 tiles thick (not 1) to
     * fit a walked exit door back to the bar -- same jamb/fill/jamb art
     * and reasoning as the bar's own doors (see show_bar_interior()). */
    for (i = 1; i < 27; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 1, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(0, 0, 1, 30, BAR_TILE_WALL);
    fill_title_rect(0, 31, 1, 30, BAR_TILE_WALL);
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }
    draw_text(25, 13, casino_exit_text);

    draw_text(2, 6, casino_sign_text);

    /* The slot machine: the bar's own jukebox tiles reused 2x2, same
     * "no CHR budget left" tradeoff as the floor/walls above. Centered
     * in the 30-wide interior (cols 1-30). */
    set_tile(10, 15, BAR_TILE_JUKEBOX_TOP);
    set_tile(10, 16, BAR_TILE_JUKEBOX_TOP);
    set_tile(11, 15, BAR_TILE_JUKEBOX_BOT);
    set_tile(11, 16, BAR_TILE_JUKEBOX_BOT);
    draw_text(7, 14, casino_slots_text);

    /* The left machine is the casino's small video-poker cabinet. It shares
     * the slot cabinet silhouette, but receives its own full-screen game
     * table when used so the player can clearly hold and redraw cards. */
    set_tile(10, 5, BAR_TILE_JUKEBOX_TOP);
    set_tile(10, 6, BAR_TILE_JUKEBOX_TOP);
    set_tile(11, 5, BAR_TILE_JUKEBOX_BOT);
    set_tile(11, 6, BAR_TILE_JUKEBOX_BOT);
    draw_text(7, 3, poker_title);

    /* Blackjack table, on the right side. No CHR budget left for
     * dedicated table art (see TOKEN_TILE's comment) -- but Neon
     * Avenue's storefront sign (see draw_storefront_sign() in
     * v2_scene_ui.c) already had a rounded-rectangle tile kit -- corners,
     * straight top/bottom edges, and a fill -- and reusing its edge
     * tiles (201/207) here still works well: a straight run of rail-over-
     * felt reads fine regardless of source. Its corner tiles, though,
     * only cut 2-3px at this scale, which didn't read as "rounded" so
     * much as "the rectangle has bevelled corners." The table's own 4
     * corners (230/231 in taxi.s, 242 in bar_interior.s, 204 in
     * neon_avenue.s -- see each file's own comment for how those slots
     * were freed) use a bolder stepped cut instead. */
    set_tile(10, 22, 230); set_tile(10, 23, 201); set_tile(10, 24, 201); set_tile(10, 25, 231);
    set_tile(11, 22, 242); set_tile(11, 23, 207); set_tile(11, 24, 207); set_tile(11, 25, 204);
    /* Row 7, not row 9 like POKER/SLOTS' labels -- row 9 falls in the same
     * 4-tile attribute band as the table itself (rows 8-11), which the
     * table needs on the green-felt palette. Sharing that band forced the
     * label onto the same white-on-green scheme instead of the room's
     * normal yellow-on-wood text, the one thing that didn't match. Row 7
     * sits in the band above, still on palette 0, so it reads exactly
     * like the other two labels while staying visually right above the
     * table it names. */
    draw_text(7, 21, casino_blackjack_text);

    /* Roulette gets the quieter lower half of the room: a broad felt table
     * beneath the machines, leaving a clear aisle around all four games. */
    set_tile(18, 11, 230); fill_title_rect(18, 12, 8, 1, 201); set_tile(18, 20, 231);
    set_tile(19, 11, 242); fill_title_rect(19, 12, 8, 1, 204); set_tile(19, 20, 204);
    set_tile(20, 11, 242); fill_title_rect(20, 12, 8, 1, 207); set_tile(20, 20, 207);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(2, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    /* The blackjack table (tile rows 10-11, cols 22-25) sits inside attr
     * row 2, spanning attr cols 5 and 6 -- those switch to palette 1 (see
     * above) so the table reads as wood rail over green felt. Its label
     * lives one attribute row up (see the draw_text() call above), so it
     * doesn't need to share this palette at all. */
    set_attribute_block(2, 5, 1);
    set_attribute_block(2, 6, 1);
    set_attribute_block(4, 2, 1);
    set_attribute_block(4, 3, 1);
    set_attribute_block(4, 4, 1);
    set_attribute_block(4, 5, 1);

    /* The reel/credits/bet HUD and its control hints deliberately aren't
     * drawn here -- the slot machine is its own full-screen game now (see
     * run_slots_game()), entered only once the player walks up and
     * presses A, instead of being visible across the whole room from
     * scene entry. */

    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

#define SLOTS_CONTROLS_ROW 21
#define SLOTS_PULL_ROW 23

/* Same full-screen shape as show_blackjack_screen()/show_poker_screen()
 * now (see run_slots_game()) rather than an overlay panel the player
 * stood on top of within the room -- the machine used to reveal only a
 * dark panel over part of the floor with the player sprite hidden
 * underneath it; now it's its own screen, consistent with the other two
 * games. The result row (see show_slots_result()) starts blank -- it's
 * only ever populated after an actual pull, never drawn here. */
static void show_slots_screen(void)
{
    show_casino_game_screen(casino_slots_text);
    draw_slots_payout_table();
    draw_slots_windows();
    draw_slots_lever(0);
    draw_casino_hud();
    draw_text(SLOTS_CONTROLS_ROW, 4, casino_controls_text);
    draw_text(SLOTS_PULL_ROW, 4, casino_pull_text);
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Same "sustained release, not just a one-frame gap" debounce as
 * show_message_box() -- a genuinely held button can still read clear for
 * a single frame (an emulator/OS auto-repeat quirk), which a plain
 * prev_pad edge check mistakes for release-then-repress, firing an
 * action again on every such blip. Returns 1 exactly once per real
 * press-then-release cycle, 0 otherwise (including every frame the
 * button stays held). held and release_run are the caller's persistent
 * per-button state, carried across frames. */


static void draw_card_rank(unsigned char row, unsigned char col, unsigned char rank)
{
    char text[3];

    if (rank == 1) {
        draw_text(row, col, "A");
    } else if (rank == 11) {
        draw_text(row, col, "J");
    } else if (rank == 12) {
        draw_text(row, col, "Q");
    } else if (rank == 13) {
        draw_text(row, col, "K");
    } else if (rank == 10) {
        draw_text(row, col, "10");
    } else {
        text[0] = (char)('0' + rank);
        text[1] = 0;
        draw_text(row, col, text);
    }
}

/* Maps a real 0-3 suit (poker_suit[], see v2_casino.h) to its tile. */
static unsigned char suit_to_tile(unsigned char suit)
{
    switch (suit) {
        case 0: return CARD_SUIT_SPADE;
        case 1: return CARD_SUIT_HEART;
        case 2: return CARD_SUIT_CLUB;
        default: return CARD_SUIT_DIAMOND;
    }
}

/* casino_blackjack_deal()/_hit() (see v2_casino.c) never track a suit --
 * only the hand total decides blackjack, on purpose. This derives one from
 * the card's rank and its position in the hand purely so each drawn card
 * looks distinct; it's cosmetic only and never read back by any game rule.
 * Poker's real, tracked suit uses suit_to_tile() directly instead. */
static unsigned char cosmetic_suit_tile(unsigned char rank, unsigned char index)
{
    return suit_to_tile((unsigned char)((rank + index) % 4));
}

/* A bordered playing card: rank in the top-left interior cell, a suit icon
 * below it. suit_tile is whatever the caller decided the suit is -- see
 * cosmetic_suit_tile() (blackjack) and suit_to_tile() (poker). */
static void draw_card_box(unsigned char row, unsigned char col, unsigned char rank, unsigned char suit_tile)
{
    fill_title_rect(row, col, CARD_WIDTH, 1, CARD_BORDER_H);
    fill_title_rect((unsigned char)(row + CARD_HEIGHT - 1), col, CARD_WIDTH, 1, CARD_BORDER_H);
    set_tile((unsigned char)(row + 1), col, CARD_BORDER_V);
    set_tile((unsigned char)(row + 2), col, CARD_BORDER_V);
    set_tile((unsigned char)(row + 1), (unsigned char)(col + CARD_WIDTH - 1), CARD_BORDER_V);
    set_tile((unsigned char)(row + 2), (unsigned char)(col + CARD_WIDTH - 1), CARD_BORDER_V);
    draw_card_rank((unsigned char)(row + 1), (unsigned char)(col + 1), rank);
    set_tile((unsigned char)(row + 2), (unsigned char)(col + 1), suit_tile);
}

/* Same frame, a woven CARD_BACK fill instead of rank/suit -- the dealer's
 * face-down card before the reveal. */
static void draw_card_back_box(unsigned char row, unsigned char col)
{
    unsigned char r, c;

    fill_title_rect(row, col, CARD_WIDTH, 1, CARD_BORDER_H);
    fill_title_rect((unsigned char)(row + CARD_HEIGHT - 1), col, CARD_WIDTH, 1, CARD_BORDER_H);
    for (r = 1; r < CARD_HEIGHT - 1; ++r) {
        for (c = 0; c < CARD_WIDTH; ++c) {
            set_tile((unsigned char)(row + r), (unsigned char)(col + c),
                     (c == 0 || c == CARD_WIDTH - 1) ? CARD_BORDER_V : CARD_BACK);
        }
    }
}

/* Leaves rendering disabled (PPU.mask stays 0) on return -- this is a
 * shared "clear the screen and draw the header" helper, not a complete
 * screen by itself. Every caller (show_blackjack_screen(),
 * show_poker_screen()) draws its own game-specific content immediately
 * after this returns, and it's THAT combined content, not just the
 * header, that needs to finish before rendering comes back on. The
 * previous version re-enabled rendering here, which meant every card,
 * every "HOLD"/"WIN"/prompt line the caller drew afterward was a raw
 * $2007 write during active rendering -- exactly the kind of corruption
 * that produced garbled tiles the first time this session hit it in the
 * slot machine (see show_casino_slot_ui()'s history). Each caller must
 * now set PPU.mask = 0x1E itself once its own drawing is done. */
static void show_casino_game_screen(const char *title)
{
    unsigned char row, col;

    PPU.mask = 0;
    /* CHR banks only change safely in forced blank, same rule as every
     * other VRAM update -- selecting a different bank mid-render would
     * swap every on-screen tile's pattern data out from under active
     * scanout. Switched back to the common bank by show_casino() (see
     * its own call to mmc3_use_common_chr()), which both run_casino()
     * call sites already re-invoke on return from a card game. */
    mmc3_use_casino_cards_chr();
    vram_queue_clear();
    /* Font tile zero is a clean solid field. Unlike the old bar-wall fill it
     * has no decorative pixels to leak through the game table or card areas. */
    fill_title_rect(0, 0, 32, 30, FONT_TILE_BASE);
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x06;
    PPU.vram.data = 0x16;
    PPU.vram.data = 0x30;
    /* Index 1 was an otherwise-unused dark green (0x0A) -- every tile on
     * this screen used only index 2 (green felt) or index 3 (white ink)
     * until the suit icons started using it for red hearts/diamonds (see
     * casino_cards.s). Spades/clubs use index 0, the universal backdrop,
     * already black and likewise not otherwise visible on this screen. */
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x16;
    PPU.vram.data = 0x1A;
    PPU.vram.data = 0x30;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) set_attribute_block(row, col, 1);
    }
    draw_text(3, 11, title);
    draw_text(5, 6, "CRED");
    draw_digits3(5, 12, casino_credits);
    draw_text(5, 18, "BET");
    draw_digits2(5, 23, casino_bet);
    clear_sprites();
    oam_flush();
}

/* Rows 7-8, not the top of the screen -- a compact two-line table rather
 * than poker's row-per-hand one (see draw_poker_payout_table()), since
 * blackjack only ever has three outcomes to show, not nine. Everything
 * below moved down 2 rows to make room, same reasoning as poker's own
 * shift, just a smaller one. */
#define BLACKJACK_PAYOUT_TABLE_TOP 7
#define BLACKJACK_DEALER_LABEL_ROW 10
#define BLACKJACK_DEALER_ROW 11
#define BLACKJACK_PLAYER_LABEL_ROW 16
#define BLACKJACK_PLAYER_ROW 18
#define CARD_ROW_START_COL 4
#define CARD_COL_SPACING 5

static void show_blackjack_screen(void)
{
    unsigned char i, col;
    unsigned char dealer_visible = game_state.casino.blackjack_state == CASINO_GAME_RESOLVED;
    /* The only way to reach 21 on exactly two cards is an ace plus a
     * ten-value card -- same natural check casino_blackjack_resolve()
     * makes (see its own comment), just re-derived here from public
     * state rather than a stored flag, since this is the only place that
     * needs to know it once the hand's already resolved. */
    unsigned char natural = (unsigned char)(game_state.casino.blackjack_player_count == 2 &&
                                             casino_blackjack_player_total(&game_state.casino) == 21);

    show_casino_game_screen(casino_blackjack_text);
    draw_text(BLACKJACK_PAYOUT_TABLE_TOP, 2, "BLACKJACK PAYS 3:2");
    draw_text((unsigned char)(BLACKJACK_PAYOUT_TABLE_TOP + 1), 2, "WIN PAYS 1:1  PUSH EVEN");
    draw_text(BLACKJACK_DEALER_LABEL_ROW, 4, "DEALER CARDS");
    if (game_state.casino.blackjack_state == CASINO_GAME_READY) {
        draw_text((unsigned char)(BLACKJACK_DEALER_LABEL_ROW + 2), 11, "WAITING FOR DEAL");
    } else {
        for (i = 0; i < game_state.casino.blackjack_dealer_count; ++i) {
            col = (unsigned char)(CARD_ROW_START_COL + i * CARD_COL_SPACING);
            if (i == 1 && !dealer_visible) draw_card_back_box(BLACKJACK_DEALER_ROW, col);
            else draw_card_box(BLACKJACK_DEALER_ROW, col, game_state.casino.blackjack_dealer[i],
                               cosmetic_suit_tile(game_state.casino.blackjack_dealer[i], i));
        }
    }
    draw_text(BLACKJACK_PLAYER_LABEL_ROW, 4, "YOUR CARDS");
    if (game_state.casino.blackjack_state == CASINO_GAME_READY) {
        draw_text((unsigned char)(BLACKJACK_PLAYER_LABEL_ROW + 1), 11, "PLACE YOUR BET");
    } else {
        for (i = 0; i < game_state.casino.blackjack_player_count; ++i) {
            col = (unsigned char)(CARD_ROW_START_COL + i * CARD_COL_SPACING);
            draw_card_box(BLACKJACK_PLAYER_ROW, col, game_state.casino.blackjack_player[i],
                          cosmetic_suit_tile(game_state.casino.blackjack_player[i], i));
        }
        draw_text(22, 6, "YOUR TOTAL");
        draw_digits2(22, 18, casino_blackjack_player_total(&game_state.casino));
    }
    if (game_state.casino.blackjack_state == CASINO_GAME_ACTIVE) {
        draw_text(24, 5, "A HIT   B STAND");
    } else {
        /* RESOLVED and READY both end up showing "A DEAL   B OUT" --
         * used to be two separate copies of that literal, one at the end
         * of each branch; hoisted out here since only the RESOLVED case
         * has anything extra (the outcome text) to show first. */
        if (game_state.casino.blackjack_state == CASINO_GAME_RESOLVED) {
            /* blackjack_payout is bet*2 on an outright win (or the natural
             * table's 3:2 figure on a natural), exactly bet on a push
             * (player just gets their own bet back, no net gain), and 0 on
             * a loss -- three outcomes, not two, now that the actual
             * number is on screen and would otherwise make a push look
             * like a mislabeled win. A natural win specifically says so
             * rather than just "YOU WIN" -- the payout number alone can't
             * tell them apart (a natural at bet 1 and a plain win at bet 1
             * both pay 2). */
            if (game_state.casino.blackjack_payout == 0) {
                draw_text(24, 11, "HOUSE WINS");
            } else if (game_state.casino.blackjack_payout == game_state.casino.bet) {
                draw_text(24, 12, "PUSH");
            } else if (natural) {
                draw_text(24, 5, "BLACKJACK! +");
                draw_digits2(24, 17, game_state.casino.blackjack_payout);
            } else {
                draw_text(24, 8, you_win_text);
                draw_digits2(24, 17, game_state.casino.blackjack_payout);
            }
        }
        draw_text(26, 5, "A DEAL   B OUT");
    }
    draw_text(28, 6, lr_bet_text);
    reset_scroll();
    PPU.mask = 0x1E;
}

static void run_blackjack_game(void)
{
    unsigned char pad;
    unsigned char a_held = 1, a_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;

    show_blackjack_screen();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (pad & PAD_START) {
            run_inventory();
            show_blackjack_screen();
            continue;
        }
        if (debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run) && casino_bet > 1) {
            --casino_bet;
            show_blackjack_screen();
        }
        if (debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run) && casino_bet < 10) {
            ++casino_bet;
            show_blackjack_screen();
        }
        if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            if (game_state.casino.blackjack_state == CASINO_GAME_ACTIVE) {
                casino_blackjack_hit(&game_state.casino);
            } else {
                casino_blackjack_deal(&game_state.casino);
            }
            show_blackjack_screen();
        }
        if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) {
            if (game_state.casino.blackjack_state == CASINO_GAME_ACTIVE) {
                casino_blackjack_stand(&game_state.casino);
                show_blackjack_screen();
            } else {
                return;
            }
        }
    }
}

/* 19, not the 10 this used to be -- the payout table (see
 * draw_poker_payout_table()) now occupies rows 7-15, so the cards moved
 * down to make room instead of overlapping it. */
#define POKER_CARD_ROW 19

/* casino_poker_result() (see v2_casino.c) returns a POKER_RESULT_* value
 * (see its enum for the NONE..ROYAL_FLUSH order this table matches);
 * poker_result_text() and draw_poker_payout_table() below both read
 * names from here instead of each spelling out its own copy of "ROYAL
 * FLUSH" etc. -- this scene's own PRG bank is tight enough that the
 * duplicated string literals were a real cost, not just tidiness. The
 * multiplier table matches casino_poker_payout()'s own (see v2_casino.c)
 * by hand, same as before. */
static const char * const poker_hand_name[10] = {
    "NO WIN", "PAIR", "TWO PAIR", "THREE OF A KIND", "STRAIGHT",
    "FLUSH", "FULL HOUSE", "FOUR OF A KIND", "STRAIGHT FLUSH", "ROYAL FLUSH"
};
static const unsigned char poker_multiplier[10] = {0, 2, 3, 4, 5, 6, 9, 15, 30, 100};

static const char *poker_result_text(unsigned char result)
{
    return poker_hand_name[result];
}

/* Name at a fixed left column, "X" plus its multiplier at a fixed right
 * column regardless of the name's own length -- simpler than actually
 * right-aligning the number, and every name here is short enough to
 * leave a clear gap before it either way. */
static void draw_payout_row(unsigned char row, const char *name, unsigned char multiplier)
{
    char buf[5];
    unsigned char i = 0;

    draw_text(row, 2, name);
    buf[i++] = 'X';
    if (multiplier >= 100) buf[i++] = (char)('0' + multiplier / 100);
    if (multiplier >= 10) buf[i++] = (char)('0' + (multiplier / 10) % 10);
    buf[i++] = (char)('0' + multiplier % 10);
    buf[i] = 0;
    draw_text(row, 22, buf);
}

#define POKER_PAYOUT_TABLE_TOP 7
#define POKER_HAND_LABEL_ROW 17

/* Walks poker_hand_name[]/poker_multiplier[] from ROYAL_FLUSH (index 9)
 * down to TWO_PAIR (index 2), best hand first -- a loop instead of 8
 * individual draw_payout_row() calls, since each explicit call site
 * used to carry its own copy of the name string. The last row (PAIR) is
 * drawn separately with the "10+" reminder, since that's specific to
 * this table and not part of poker_result_text()'s plain "PAIR". */
static void draw_poker_payout_table(void)
{
    unsigned char i;

    for (i = 0; i < 8; ++i) {
        draw_payout_row((unsigned char)(POKER_PAYOUT_TABLE_TOP + i),
                         poker_hand_name[9 - i], poker_multiplier[9 - i]);
    }
    draw_payout_row((unsigned char)(POKER_PAYOUT_TABLE_TOP + 8), "PAIR 10+", poker_multiplier[1]);
}

/* Moves the single cursor tile above the hand without touching anything
 * else on screen -- run_poker_game() uses this (and queue_poker_hold()
 * below) instead of a full show_poker_screen() redraw for LEFT/RIGHT/A,
 * since those only ever change 1-4 tiles. A full forced-blank redraw on
 * every cursor nudge was the actual cause of the reported flicker; queued
 * writes land between frames via the NMI drain (see v2_vram_queue.c)
 * instead of blanking the screen to make them. */
static void queue_poker_cursor(unsigned char old_selected, unsigned char new_selected)
{
    unsigned char old_col = (unsigned char)(CARD_ROW_START_COL + old_selected * CARD_COL_SPACING + 1);
    unsigned char new_col = (unsigned char)(CARD_ROW_START_COL + new_selected * CARD_COL_SPACING + 1);
    (void)vram_queue_tile((unsigned char)(POKER_CARD_ROW - 1), old_col, FONT_TILE_BASE);
    (void)vram_queue_tile((unsigned char)(POKER_CARD_ROW - 1), new_col, CARD_BORDER_V);
}

/* Draws or clears the 4-tile "HOLD" label under one card in place. */
static void queue_poker_hold(unsigned char index, unsigned char held)
{
    unsigned char col = (unsigned char)(CARD_ROW_START_COL + index * CARD_COL_SPACING);
    unsigned char row = (unsigned char)(POKER_CARD_ROW + CARD_HEIGHT);
    const char *text = held ? "HOLD" : "    ";
    unsigned char i;
    for (i = 0; i < 4; ++i) {
        (void)vram_queue_tile(row, (unsigned char)(col + i),
                              (unsigned char)(FONT_TILE_BASE + font_glyph_index(text[i])));
    }
}

static void show_poker_screen(unsigned char selected)
{
    unsigned char i, col;

    show_casino_game_screen(poker_title);
    draw_poker_payout_table();
    draw_text(POKER_HAND_LABEL_ROW, 4, "HAND");
    if (game_state.casino.poker_state == CASINO_GAME_READY) {
        draw_text((unsigned char)(POKER_HAND_LABEL_ROW + 3), 7, "A DEAL");
    } else {
        for (i = 0; i < CASINO_HAND_SIZE; ++i) {
            col = (unsigned char)(CARD_ROW_START_COL + i * CARD_COL_SPACING);
            draw_card_box(POKER_CARD_ROW, col, game_state.casino.poker_rank[i],
                          suit_to_tile(game_state.casino.poker_suit[i]));
            if (game_state.casino.poker_state == CASINO_GAME_ACTIVE &&
                (game_state.casino.poker_held & (1 << i))) {
                draw_text((unsigned char)(POKER_CARD_ROW + CARD_HEIGHT), col, "HOLD");
            }
            /* A single cursor tile sitting just above the selected card,
             * borrowing the same card-frame tile as a simple pointer --
             * there's no separate "highlighted border" art. */
            if (game_state.casino.poker_state == CASINO_GAME_ACTIVE && i == selected) {
                set_tile((unsigned char)(POKER_CARD_ROW - 1), (unsigned char)(col + 1), CARD_BORDER_V);
            }
        }
        if (game_state.casino.poker_state == CASINO_GAME_RESOLVED) {
            draw_text(25, 4, poker_result_text(casino_poker_result(&game_state.casino)));
            if (game_state.casino.poker_payout) {
                draw_text(26, 6, you_win_text);
                draw_digits3(26, 15, game_state.casino.poker_payout);
            } else {
                draw_text(26, 6, "NO PAYOUT");
            }
        }
    }
    if (game_state.casino.poker_state == CASINO_GAME_ACTIVE) {
        draw_text(28, 3, "A HOLD B GO L R PICK");
    } else {
        draw_text(27, 6, lr_bet_text);
        draw_text(28, 4, "A DEAL B OUT");
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void run_poker_game(void)
{
    unsigned char pad;
    unsigned char selected = 0;
    unsigned char a_held = 1, a_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;

    show_poker_screen(selected);
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (pad & PAD_START) {
            run_inventory();
            show_poker_screen(selected);
            continue;
        }
        if (game_state.casino.poker_state == CASINO_GAME_ACTIVE) {
            /* Cursor move and hold toggle only ever change a handful of
             * tiles -- queued in place instead of a full show_poker_screen()
             * redraw, so picking cards to hold no longer flashes the whole
             * screen blank on every press. */
            if (debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run)) {
                unsigned char old_selected = selected;
                selected = selected ? (unsigned char)(selected - 1) : (CASINO_HAND_SIZE - 1);
                queue_poker_cursor(old_selected, selected);
            }
            if (debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run)) {
                unsigned char old_selected = selected;
                selected = (unsigned char)((selected + 1) % CASINO_HAND_SIZE);
                queue_poker_cursor(old_selected, selected);
            }
            if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
                game_state.casino.poker_held ^= (unsigned char)(1 << selected);
                queue_poker_hold(selected, (unsigned char)((game_state.casino.poker_held >> selected) & 1));
            }
            if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) {
                casino_poker_draw(&game_state.casino);
                show_poker_screen(selected);
            }
        } else {
            if (debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run) && casino_bet > 1) {
                --casino_bet;
                show_poker_screen(selected);
            }
            if (debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run) && casino_bet < 10) {
                ++casino_bet;
                show_poker_screen(selected);
            }
            if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
                casino_poker_deal(&game_state.casino);
                show_poker_screen(selected);
            }
            if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) return;
        }
    }
}

/* Same full-screen shape as run_blackjack_game()/run_poker_game() now
 * (see show_slots_screen()'s comment) -- entered and exited the same way,
 * B backing all the way out to the casino floor instead of a nested
 * walking/slot-UI mode within one function. The reel spin itself is
 * unique to this game: display_reel steps toward the resolved result a
 * little at a time, redrawn through the NMI queue (see queue_casino_hud())
 * rather than a forced-blank redraw per step, since it advances every few
 * frames for a couple of seconds and a forced-blank flash on every one of
 * those steps would be a visibly flickering spin instead of a smooth one. */
static void run_slots_game(void)
{
    unsigned char pad;
    unsigned char display_reel[3];
    unsigned char reel_spin_active = 0;
    unsigned char reel_spin_tick = 0;
    unsigned char reel_spin_step = 0;
    unsigned char lever_pulled = 0;
    unsigned char hud_dirty = 0;
    unsigned char a_held = 1, a_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;

    display_reel[0] = casino_reel[0];
    display_reel[1] = casino_reel[1];
    display_reel[2] = casino_reel[2];
    show_slots_screen();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();

        /* Deferred from last frame's bet/pull handling. The HUD's dynamic
         * tile writes fit inside the fixed 32-entry queue, so NMI owns the
         * transaction and no blank frame is needed for bet or reel changes. */
        if (hud_dirty) {
            queue_casino_hud(display_reel, lever_pulled);
            hud_dirty = 0;
        }

        pad = read_pad();

        if (pad & PAD_START) {
            run_inventory();
            show_slots_screen();
            hud_dirty = 1;
            continue;
        }

        if (reel_spin_active) {
            /* Reels share the HUD batch, so advancing once every three
             * frames stays well within NMI's vblank budget. They stop in
             * sequence instead of all snapping to the already-resolved
             * result at once. Input stays locked until the last reel
             * settles. */
            if (++reel_spin_tick == 3) {
                unsigned char reel;

                reel_spin_tick = 0;
                ++reel_spin_step;
                for (reel = 0; reel < 3; ++reel) {
                    if (reel_spin_step < (unsigned char)(5 + reel * 2)) {
                        display_reel[reel] = (unsigned char)((display_reel[reel] + 1) % CASINO_SYMBOL_COUNT);
                    } else {
                        display_reel[reel] = casino_reel[reel];
                    }
                }
                hud_dirty = 1;
                /* Must match the last reel's own settle threshold above
                 * (5 + reel*2, reel==2 => 9) -- this used to read >= 8,
                 * one tick early, so the animation could stop and show
                 * the result text (which reads the real casino_reel[])
                 * while reel 2's last queued HUD frame was still showing
                 * an intermediate spinning value instead of its true
                 * symbol, letting the reels and the result text visibly
                 * disagree. */
                if (reel_spin_step >= 9) {
                    reel_spin_active = 0;
                    lever_pulled = 0;
                    /* One-shot, not queued -- see show_slots_result()'s
                     * own comment on why. */
                    show_slots_result();
                }
            }
        } else {
            if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) return;
            if (debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run)) {
                if (casino_bet > 1) --casino_bet;
                hud_dirty = 1;
            }
            if (debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run)) {
                if (casino_bet < 10) ++casino_bet;
                hud_dirty = 1;
            }
            if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run) &&
                casino_credits >= casino_bet) {
                casino_spin(&game_state.casino);
                reel_spin_active = 1;
                reel_spin_tick = 0;
                reel_spin_step = 0;
                lever_pulled = 1;
                hud_dirty = 1;
            }
        }
    }
}

/* A proper American layout: two green pockets and a three-column board for
 * 1-36. The same standard wheel order used by v2_casino.c drives the ball,
 * so the animated marker always stops on the resolved pocket rather than a
 * merely decorative loop. */
static void roulette_pocket_text(char *text, unsigned char pocket)
{
    if (pocket == CASINO_ROULETTE_DOUBLE_ZERO) {
        text[0] = '0'; text[1] = '0'; text[2] = 0;
    } else if (pocket >= 10) {
        text[0] = (char)('0' + pocket / 10);
        text[1] = (char)('0' + pocket % 10);
        text[2] = 0;
    } else {
        text[0] = (char)('0' + pocket); text[1] = 0;
    }
}

/* The marker lives in the blank first tile of each pocket cell, leaving the
 * numeric label and its red/black suit icon untouched throughout a spin. */
static void roulette_pocket_position(unsigned char pocket, unsigned char *row, unsigned char *col)
{
    if (pocket == CASINO_ROULETTE_ZERO) {
        *row = 8; *col = 7;
    } else if (pocket == CASINO_ROULETTE_DOUBLE_ZERO) {
        *row = 8; *col = 16;
    } else if (pocket == CASINO_ROULETTE_RED) {
        *row = 21; *col = 7;
    } else if (pocket == CASINO_ROULETTE_BLACK) {
        *row = 21; *col = 17;
    } else {
        pocket = (unsigned char)(pocket - 1);
        *row = (unsigned char)(9 + pocket / 3);
        *col = (unsigned char)(5 + (pocket % 3) * 9);
    }
}

static void draw_roulette_pocket(unsigned char row, unsigned char col, unsigned char pocket)
{
    char text[3];
    unsigned char icon;

    fill_title_rect(row, col, 8, 1, FONT_TILE_BASE);
    roulette_pocket_text(text, pocket);
    draw_text(row, (unsigned char)(col + 1), text);
    if (pocket == CASINO_ROULETTE_ZERO || pocket == CASINO_ROULETTE_DOUBLE_ZERO) icon = CARD_SUIT_CLUB;
    else icon = (pocket & 1) ? CARD_SUIT_HEART : CARD_SUIT_SPADE;
    set_tile(row, (unsigned char)(col + 5), icon);
}

static void draw_roulette_board(unsigned char marker)
{
    unsigned char row, col, pocket;
    unsigned char marker_row, marker_col;

    draw_roulette_pocket(8, 7, CASINO_ROULETTE_ZERO);
    draw_roulette_pocket(8, 16, CASINO_ROULETTE_DOUBLE_ZERO);
    for (row = 0; row < 12; ++row) {
        for (col = 0; col < 3; ++col) {
            pocket = (unsigned char)(row * 3 + col + 1);
            draw_roulette_pocket((unsigned char)(row + 9), (unsigned char)(5 + col * 9), pocket);
        }
    }
    draw_text(21, 8, "RED");
    set_tile(21, 12, CARD_SUIT_HEART);
    draw_text(21, 18, "BLACK");
    set_tile(21, 24, CARD_SUIT_SPADE);
    roulette_pocket_position(marker, &marker_row, &marker_col);
    set_tile(marker_row, marker_col, CARD_SUIT_DIAMOND);
}

static void show_roulette_screen(unsigned char marker)
{
    char text[3];

    show_casino_game_screen("ROULETTE");
    draw_roulette_board(marker);
    draw_text(25, 4, "SELECT BET");
    if (game_state.casino.roulette_state == CASINO_GAME_RESOLVED) {
        draw_text(23, 7, "RESULT");
        roulette_pocket_text(text, game_state.casino.roulette_result);
        draw_text(23, 15, text);
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void queue_roulette_marker(unsigned char old_pocket, unsigned char new_pocket)
{
    unsigned char old_row, old_col, new_row, new_col;
    roulette_pocket_position(old_pocket, &old_row, &old_col);
    roulette_pocket_position(new_pocket, &new_row, &new_col);
    (void)vram_queue_tile(old_row, old_col, FONT_TILE_BASE);
    (void)vram_queue_tile(new_row, new_col, CARD_SUIT_DIAMOND);
}

static unsigned char roulette_move(unsigned char pocket, unsigned char direction)
{
    unsigned char row, col;

    if (pocket == CASINO_ROULETTE_ZERO) {
        if (direction == PAD_RIGHT) return CASINO_ROULETTE_DOUBLE_ZERO;
        if (direction == PAD_DOWN) return 1;
        return pocket;
    }
    if (pocket == CASINO_ROULETTE_DOUBLE_ZERO) {
        if (direction == PAD_LEFT) return CASINO_ROULETTE_ZERO;
        if (direction == PAD_DOWN) return 2;
        return pocket;
    }
    if (pocket == CASINO_ROULETTE_RED) {
        if (direction == PAD_RIGHT) return CASINO_ROULETTE_BLACK;
        if (direction == PAD_UP) return 34;
        return pocket;
    }
    if (pocket == CASINO_ROULETTE_BLACK) {
        if (direction == PAD_LEFT) return CASINO_ROULETTE_RED;
        if (direction == PAD_UP) return 36;
        return pocket;
    }
    pocket = (unsigned char)(pocket - 1);
    row = (unsigned char)(pocket / 3);
    col = (unsigned char)(pocket % 3);
    if (direction == PAD_UP) return row ? (unsigned char)(pocket - 2) : (col ? CASINO_ROULETTE_DOUBLE_ZERO : CASINO_ROULETTE_ZERO);
    if (direction == PAD_DOWN) {
        if (row < 11) return (unsigned char)(pocket + 4);
        return col < 2 ? CASINO_ROULETTE_RED : CASINO_ROULETTE_BLACK;
    }
    if (direction == PAD_LEFT) return col ? pocket : (unsigned char)(pocket + 3);
    return col < 2 ? (unsigned char)(pocket + 2) : (unsigned char)(pocket - 1);
}

static unsigned char roulette_wheel_index(unsigned char pocket)
{
    unsigned char i;
    for (i = 0; i < CASINO_ROULETTE_POCKET_COUNT; ++i) {
        if (casino_roulette_wheel[i] == pocket) return i;
    }
    return 0;
}

static void run_roulette_game(void)
{
    unsigned char pad;
    unsigned char pick = game_state.casino.roulette_pick;
    unsigned char ball = pick;
    unsigned char wheel_index;
    unsigned char spin_tick = 0;
    unsigned char spin_steps = 0;
    unsigned char target_steps = 0;
    unsigned char spinning = 0;
    unsigned char a_held = 1, a_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char direction_held = 0;
    unsigned char select_held = 0, select_run = 0;

    if (pick >= CASINO_ROULETTE_BET_COUNT) pick = 1;
    show_roulette_screen(game_state.casino.roulette_state == CASINO_GAME_RESOLVED ? game_state.casino.roulette_result : pick);
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();

        if (spinning) {
            if (++spin_tick == 2) {
                unsigned char old_ball = ball;
                spin_tick = 0;
                wheel_index = (unsigned char)((wheel_index + 1) % CASINO_ROULETTE_POCKET_COUNT);
                ball = casino_roulette_wheel[wheel_index];
                queue_roulette_marker(old_ball, ball);
                if (++spin_steps == target_steps) {
                    spinning = 0;
                    show_roulette_screen(game_state.casino.roulette_result);
                }
            }
            continue;
        }

        if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) return;
        if ((pad & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) && !direction_held) {
            unsigned char old_pick = pick;
            pick = roulette_move(pick, (unsigned char)(pad & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)));
            if (pick != old_pick) queue_roulette_marker(old_pick, pick);
        }
        direction_held = (unsigned char)(pad & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT));
        if (debounce_press((unsigned char)(pad & PAD_SELECT), &select_held, &select_run)) {
            casino_bet = casino_bet == 10 ? 1 : (unsigned char)(casino_bet + 1);
            queue_digits2(5, 23, casino_bet);
        }
        if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run) && casino_credits >= casino_bet) {
            casino_roulette_spin(&game_state.casino, pick);
            ball = pick < CASINO_ROULETTE_POCKET_COUNT ? pick : CASINO_ROULETTE_ZERO;
            wheel_index = roulette_wheel_index(ball);
            spin_tick = 0;
            spin_steps = 0;
            target_steps = (unsigned char)(76 + roulette_wheel_index(game_state.casino.roulette_result) - wheel_index);
            spinning = 1;
        }
    }
}

/* Fixtures are solid, but the row below each remains open for interaction. */
static unsigned char casino_fixture_blocks(unsigned char x, unsigned char y)
{
    if (x >= 24 && x <= 72 && y >= 72 && y <= 104) return 1;   /* Video poker */
    if (x >= 104 && x <= 152 && y >= 72 && y <= 104) return 1; /* Slots */
    if (x >= 168 && x <= 216 && y >= 72 && y <= 104) return 1; /* Blackjack */
    if (x >= 88 && x <= 176 && y >= 136 && y <= 168) return 1; /* Roulette */
    return 0;
}

/* Same flush-first shape as the other two scenes' loops. All three games
 * are their own full-screen modes now (see run_blackjack_game(),
 * run_poker_game(), run_slots_game()); walking the floor just decides
 * which one A enters. There's no button-driven exit from the room itself
 * -- walking into the south door is the only way back to the bar, the
 * same as every other door in this game. */
void run_casino(void)
{
    unsigned char x = 128;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char pad;
    unsigned char at_machine;
    unsigned char at_blackjack;
    unsigned char at_poker;
    unsigned char at_roulette;
    unsigned char a_held = 0, a_run = 0;

    music_select_song(MUSIC_SONG_CASINO);
    show_casino();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();

        pad = read_pad();
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;

        if (pad & PAD_START) {
            run_inventory();
            show_casino();
            continue;
        }

        /* Centered on the relocated 2x2 machine (rows 10-11, cols
         * 15-16 -- pixels 120-135, 80-95), with room to approach. */
        at_machine = (x >= 96 && x <= 160 && y >= 64 && y <= 112);
        at_poker = (x >= 24 && x <= 88 && y >= 64 && y <= 112);
        at_blackjack = (x >= 168 && x <= 236 && y >= 64 && y <= 120);
        at_roulette = (x >= 80 && x <= 184 && y >= 120 && y <= 176);

        if (at_poker && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            run_poker_game();
            show_casino();
        } else if (at_blackjack && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            run_blackjack_game();
            show_casino();
        } else if (at_machine && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            run_slots_game();
            show_casino();
        } else if (at_roulette && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            run_roulette_game();
            show_casino();
        } else {
            if (pad & PAD_LEFT) {
                if (x > 20 && !casino_fixture_blocks(x - 1, y)) --x;
            } else if (pad & PAD_RIGHT) {
                if (x < 236 && !casino_fixture_blocks(x + 1, y)) ++x;
            }
            if (pad & PAD_UP) {
                if (y > 40 && !casino_fixture_blocks(x, y - 1)) --y;
            } else if (pad & PAD_DOWN) {
                /* No more B-to-exit (see this function's header comment)
                 * -- walking into the south door, the same way every
                 * other door in this game works, is now the only way
                 * back to the bar. */
                if (y < 216) {
                    if (!casino_fixture_blocks(x, y + 1)) ++y;
                } else if (x >= 104 && x <= 144) {
                    return;
                }
            }
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }

        draw_player_sprite(x, y, walk_frame, facing);
    }
}
