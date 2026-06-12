# How to publish a release

RHYTHMY uses GitHub Actions. You do **not** build anything on your PC for a
release — GitHub builds it in the cloud and publishes the files automatically.

## What happens automatically

- **Every push / pull request** → GitHub checks the code still compiles
  (Linux + Windows). This is the "CI" workflow. Nothing is published.
- **When you push a version tag** (like `v1.0.0`) → GitHub builds the R36S
  installers and a Windows zip, then creates a **Release** on the
  [Releases page](https://github.com/NHsBeat/RHYTHMY/releases) with the files
  attached. This is the "Release" workflow.

## To cut a new release

Open PowerShell in the project folder and run (replace the version number):

```powershell
cd C:\Users\Haz\Desktop\projects\RHYTHMY
git tag v1.0.0
git push origin v1.0.0
```

That's it. Go to the **Actions** tab on GitHub to watch the build
(takes ~10–15 minutes, mostly the R36S sysroot). When it finishes, the new
release with the installer files appears on the Releases page.

## Version numbers

Use the format `vMAJOR.MINOR.PATCH`:

- `v1.0.0` — first public release
- `v1.0.1` — small fix
- `v1.1.0` — new features
- `v2.0.0` — big changes

## If a build fails

Open the **Actions** tab, click the failed run (red ✗), and read the log.
The most fragile step is "Build Debian Buster arm64 sysroot" — if
`archive.debian.org` is temporarily down, just re-run the job
(button: "Re-run failed jobs").

## Deleting / re-doing a release

If you tagged the wrong thing:

```powershell
git push --delete origin v1.0.0   # remove the tag from GitHub
git tag -d v1.0.0                 # remove it locally
```

Then delete the Release on the Releases page (if it was created) and tag again.
