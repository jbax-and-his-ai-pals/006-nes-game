# World Map

This is the canonical town layout reference for future content. It describes
story geography, not literal scrolling space: each box can be one or more
fixed-camera NES scenes connected by explicit routes.

```text
                                  NORTH

                    [ Moonbeam Chapel ]
                             |
 [ Slug Acres ] -- [ Moonlight Park ]     [ Pocket Zoo ]
       |                     |                  ^
       +------------- [ Neon Avenue ] ---- closed public gate
                            |       \
        [ Galaxy Lanes
          & Arcade   ] -- [ Municipal Gym ]
               |
   [ Pawn Shop /     [ All-Nite Mart ] [ Jimbles's Bar ] -- [ The Commentarium ]
     Laundromat ] --                          |
                                    [ Wrecked 'Em Casino ]
                                              |
                                  [ Starlight Lodge ]
                                              |
                              [ Municipal Research Annex ]

                     Underground: Sewer Network
       Avenue manhole -- Park service hatch -- Zoo service hatch -- Annex utility room
```

## Current Playable Area

| Location | Surface connection | Current purpose |
| --- | --- | --- |
| Neon Avenue | Central hub | Connects the Bar, Mart, Park, and sewer. |
| Jimbles's Bar | East/center Avenue entrance | Tavern hub, Piss Corner, casino backroom door, and a side door to the Commentarium. |
| Wrecked 'Em Casino | Bar backroom door | Slots, blackjack, video poker, roulette, and a return to the Bar. |
| The Commentarium | Bar side door | A stage performer and a rotating feed of comedic "dumb comments" from the audience. |
| All-Nite Mart | West Avenue exit | Convenience-store exterior/interior and early item pickup. |
| Pawn Shop / Laundromat side street | West Mart exit | Two exteriors sharing one screen, both doors reading "not open yet." The gap between them leads north to Galaxy Lanes & Arcade, capped by a collidable horizon line -- not an open placeholder anymore. |
| Galaxy Lanes & Arcade | North path from the side street | Exterior only for now -- the door reads "not open yet." Interior (darts, shooting gallery, bowling, skeeball, whack-a-mole, claw machine) is still planned, see `ROADMAP.md`. Its east edge now leads to the Municipal Gym, though the facade art there hasn't caught up yet. |
| Municipal Gym | East edge of Galaxy Lanes & Arcade | Exterior and interior built, with the interior's own CHR page (real backboard/rim/net, wood-court floor) and the bar's music cue reused for now. The coach is a placeholder for the planned live 1-on-1 basketball challenge, see `ROADMAP.md`. |
| Moonlight Park | East Avenue exit | Fountain, wandering NPCs, and a service-hatch route toward the Zoo. |
| Sewer Network | Avenue manhole | First underground route; its dry-tunnel east exit now reaches the Zoo service hatch. |
| Pocket Zoo service/frontage | Sewer service hatch | Clear the slug infestation, meet Keeper Nibs, receive the Zoo Map, open the public gate, and enter the pond and reptile exhibits. |

## Expansion Order

1. **Moonlight Park north exit -> Moonbeam Chapel.** A quiet contrast to the
   bar/casino and a natural first story-gated destination.
2. **Park service hatch -> Pocket Zoo.** The Zoo's public gate is closed for
   supposedly routine "maintenance". The first visit currently enters through
   the sewer's Zoo service hatch; the Park hatch will become the convenient
   surface counterpart after its route is built.
3. **Park west exit -> Slug Acres.** Creates the stranger outer-town branch
   without overloading Avenue.
4. **Bar/casino south route -> Starlight Lodge.** Keeps nightlife venues near
   each other and creates a social-access chain.
5. **Lodge south route -> Municipal Research Annex.** A deliberate late-game
   destination that can also gain a gated sewer connection. (Galaxy Lanes &
   Arcade no longer sits on this chain -- it's reached north from the Pawn
   Shop/Laundromat side street instead; see Current Playable Area above.)

## Routing Rules

- Neon Avenue remains the first hub, but should not become the only hub.
- Park is the north/outdoor hub; Bar/Casino/Lodge are the nightlife hub.
- Every new surface destination should have one primary neighbor and, where
  useful, one locked shortcut or return connection.
- The sewer is a shortcut layer, not a mandatory maze. Each new sewer access
  needs a clear surface counterpart and an explicit respawn route.
- Pocket Zoo deliberately breaks the usual surface-entry rule: its first
  entry is the service hatch, while its public gate remains a future shortcut
  reward rather than an initial Park exit.
- Avoid direct routes from every location to every other location. A readable
  town should have purposeful travel and a few satisfying unlockable loops.

## Scene Budget Suggestion

Reserve one PRG pair for a simple exterior or interior, and two or more pairs
for a destination with multiple rooms or a minigame. The current manifest has
PRG pairs beginning at 25 available for this expansion map (see
`assets/banks/manifest.json` for the exact assignments already in use). Give
each new destination a dedicated CHR bank before adding substantial new art.
