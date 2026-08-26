# Scene Routing

`V2GameState.route` is the fixed-engine source of truth for the active
location and its arrival spawn. A scene owns local exit codes; the dispatcher
translates each exit to a `V2SceneRoute` before invoking the next scene.

New destinations should add a `V2SceneId`, define their local spawn values,
reserve a PRG pair in `assets/banks/manifest.json`, and set a route before
calling their banked wrapper. Death events should set the desired respawn route
instead of directly editing another scene's coordinates.

Current route/spawn conventions:

| Scene | Spawn values |
| --- | --- |
| Avenue | 0 fresh, 1 sewer, 2 Mart, 3 Park |
| Bar | 0 south entrance, 1 casino door |
| Mart | 0 avenue, 1 store exit, 2 interior, 3 side street |
| Arcade | 0 default (east edge, from the side street's north path) |
| Park, Sewer, Casino, Side Street | 0 default |
