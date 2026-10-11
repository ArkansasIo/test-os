# AetherOS Nexus boot / login UI

A standalone responsive front-end prototype inspired by the supplied Linux login-screen reference. It gives the OS a new name, **AetherOS Nexus**, and an original star-compass logo.

## Files
- `index.html` — semantic boot/login screen markup and controls.
- `styles.css` — responsive glass panels, planetary background, typography, progress bar, and reduced-motion support.
- `app.js` — clock, simulated POST sequence, progress state, session selector, and safe demo button interactions.
- `assets/aetheros-mark.svg` — original vector logo and favicon.
- `TESTING.md` — manual smoke-test checklist.

## Run
Open `index.html` in a modern browser, or serve the repository root:

```sh
python3 -m http.server 8000
```

Then visit `http://localhost:8000/ui/aetheros-boot/`.

## Important limitations
This is a **visual prototype**, not a Linux display manager, firmware interface, or kernel bootloader. POST results and progress are simulated. Login does not authenticate users, and shutdown/restart buttons do not control the host computer. Do not enter a real password.

The background is drawn with CSS gradients and shapes, so the prototype has no binary image dependencies or external image hosting.
