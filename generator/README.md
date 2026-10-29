# wlcrosshair generator

A local React, Vite, and TypeScript companion for building transparent PNG crosshairs.

## Run

```sh
cd generator
bun install
bun run dev
```

For a production build, run `bun run build`. Run `bun run format` to format TypeScript, CSS, and project files with Prettier; `bun run format:check` checks formatting.

The editor scales the design to fit. Fullscreen preview shows it at 100% CSS pixel size. The PNG uses the smallest canvas centered on the design origin that contains every visible layer, including offsets, rotation, strokes, and borders. Set `path` in your `wlcrosshair` config to the downloaded PNG.

Fullscreen preview backgrounds are local HUD-free screenshots from [Counter-Strike 2 Dust II](https://www.counter-strike.net/cs2), [VALORANT Ascent](https://playvalorant.com/en-gb/maps/), and [Deadlock](https://store.steampowered.com/app/1422450/Deadlock/). The live editor uses a plain background. Screenshots are not included in exported PNGs.

Share codes store layer settings as copyable text. They work without a server, but are unavailable when the design contains a custom image layer. Uploaded images stay in the browser session and are included in PNG exports.

## GitHub Pages

The repository includes a Bun-based Pages workflow in `.github/workflows/generator-pages.yml`. It builds this directory and deploys `generator/dist` when changes are pushed to `dev`.

1. In `marzeq/wlcrosshair`, open **Settings → Pages** and set **Build and deployment → Source** to **GitHub Actions**.
2. Commit and push the generator and workflow to `dev`.
3. Watch the **Deploy generator to GitHub Pages** workflow under **Actions**. The site URL for this repository is `https://marzeq.github.io/wlcrosshair/`.

`vite.config.ts` uses `/wlcrosshair/` as the base path for that repository URL. If the site moves to a root Pages domain such as `https://wlcrosshair.github.io/`, change `base` to `/` and update the workflow for the new repository.
