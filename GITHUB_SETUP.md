# Putting the split screen build on your GitHub

How to publish this `ges-splitscreen` branch as your own fork of GoldenEye: Source, and give
players a download. You only need a GitHub account and Git for Windows.

## 1. Fork the repository

1. Sign in at [github.com](https://github.com).
2. Open [goldeneye-source/ges-code](https://github.com/goldeneye-source/ges-code) and click
   **Fork** (top right).
3. Owner: your account. Name: `ges-code` or another name (this build's fork is
   `ges-code-Couch-Party`). Click **Create fork**.

Your fork is now at `https://github.com/<your-name>/<fork name>`. This build lives at
[T3chnobone/ges-code-Couch-Party](https://github.com/T3chnobone/ges-code-Couch-Party), branch
`ges-splitscreen`.

## 2. Push the split screen branch

Open **Git Bash** (or a terminal) in the folder that holds this branch (on the PC it was made on:
`F:\claude\ges-upstream`) and run:

```
git checkout ges-splitscreen
git remote add fork https://github.com/<your-name>/<fork name>.git
git push -u fork ges-splitscreen
```

The first push opens a browser window asking you to sign in to GitHub (Git Credential Manager).
Sign in and allow it; later pushes won't ask again.

Check on GitHub: the branch selector on your fork now lists `ges-splitscreen`.

## 3. Make it what visitors see first

On your fork: **Settings > General > Default branch**, click the switch icon, pick
`ges-splitscreen`, then **Update**. The front page then shows this branch's README, with links to
[INSTALL_SPLITSCREEN.md](INSTALL_SPLITSCREEN.md) and [SPLITSCREEN.md](SPLITSCREEN.md).

Optional, on the front page's **About** (gear icon): a description such as "GoldenEye: Source
with local split screen (up to 8 players)" and topics like `goldeneye`, `source-engine`,
`split-screen`.

## 4. Give players a download (Release)

Players shouldn't have to build anything. Put the ready-made files in a Release:

1. On your fork: **Releases > Draft a new release** (right side of the front page).
2. **Choose a tag**: type a new one, for example `splitscreen-2026-11-14`, and pick
   `ges-splitscreen` as the target.
3. Title, for example "Split screen build 2026-11-14". In the description, link
   [INSTALL_SPLITSCREEN.md](INSTALL_SPLITSCREEN.md) and list what changed.
4. Drag the release zip into **Attach binaries**: `ges-splitscreen-<date>.zip` (on the PC it was
   made on: `F:\claude\release\`). It holds `bin\client.dll`, `bin\server.dll`, `bin\SDL2.dll`,
   the changed resource files and the guides.
5. **Publish release**.

Players then download the zip from **Releases** and follow "For players" in
[INSTALL_SPLITSCREEN.md](INSTALL_SPLITSCREEN.md).

## 5. Publishing an update later

1. Make and build the change (see "For builders" in INSTALL_SPLITSCREEN.md).
2. Commit and push:
   ```
   git add -A
   git commit -m "Describe the change"
   git push
   ```
3. Make a new release zip (same files, newly built DLLs) and draft a new Release with a new tag.

To take in new changes from the official GE:S code:

```
git remote add upstream https://github.com/goldeneye-source/ges-code.git
git fetch upstream
git merge upstream/develop
git push
```

## Rules to keep

- **Licences**: GE:S's own code (the `ge_` files) is GPL v3 ([LICENSE_GES](LICENSE_GES)); the rest
  is under Valve's Source SDK licence. Keep the licence files, keep it free.
- **Don't upload** Valve's game files (anything from Source SDK Base 2013), GoldenEye: Source's
  game assets (maps, models, sounds, textures), or your own `cfg` and profile files. Only the DLLs
  built from this code, `SDL2.dll` (zlib licence) and the changed text resource files go in a
  release.
- Say clearly that it's an **unofficial** build, so players report split screen bugs to you, not
  to the GE:S team.
- To offer the work to the GE:S team: on your fork, **Contribute > Open pull request** against
  `goldeneye-source/ges-code`. Their README says they currently only take bug and build fixes.
