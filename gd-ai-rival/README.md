# AI Rival: talk to an AI while you play Geometry Dash

A Geode mod for Geometry Dash 2.2081 (Windows and Android) that adds an AI rival to your levels. You can chat with it, and it reacts on its own to your deaths, new bests and completions. Replies show up in a speech bubble that fades out after a few seconds.

```
GD + mod  --HTTPS-->  your proxy (Cloudflare Worker)  --API key-->  Claude API
```

The mod only ever talks to **your proxy**. The Anthropic API key lives in the proxy as a Cloudflare secret, never in the mod.

| Folder | What it is |
|---|---|
| `mod/` | Geode mod (C++23, Geode SDK v5.11) |
| `proxy/` | Cloudflare Worker that holds the key, rate-limits and forwards to Claude |
| `../.github/workflows/build-ai-rival-mod.yml` | GitHub Actions build for Windows + Android |

---

## 1. Deploy the proxy

You need Node.js 18+, a free [Cloudflare account](https://dash.cloudflare.com/sign-up) and an API key from the [Claude Console](https://platform.claude.com/).

```sh
cd gd-ai-rival/proxy
npm install
npx wrangler login                         # opens the browser once
npx wrangler secret put ANTHROPIC_API_KEY  # paste your key when asked
npx wrangler deploy
```

`deploy` prints your URL, e.g. `https://gd-ai-rival.<you>.workers.dev`. The endpoint the mod needs is that URL **plus `/chat`**. Quick test:

```sh
curl -X POST https://gd-ai-rival.<you>.workers.dev/chat \
  -H "Content-Type: application/json" \
  -d '{"player_id":"testplayer123","level_id":0,"message":"[PLAYER] hi (attempt 1, best 0%)"}'
# {"reply":"..."}
```

### Proxy settings

Edit `vars` in `proxy/wrangler.jsonc`, then run `npx wrangler deploy` again.

| Variable | Default | Meaning |
|---|---|---|
| `MODEL` | `claude-opus-5-5` | Claude model. `claude-haiku-5-5` is much cheaper (see costs). |
| `ALLOWED_LEVEL_IDS` | empty | Comma-separated level IDs the proxy answers for. Empty = all. **This is the real level lock** for a public mod, because the in-game setting can be changed by players. |
| `ALLOW_CUSTOM_PERSONA` | `true` | `false` ignores the player's Personality setting and always uses `DEFAULT_PERSONA`. |
| `DEFAULT_PERSONA` | the rival | Character used when custom personas are off or empty. |
| `PLAYER_PER_MINUTE` | `8` | Max requests per player per minute. |
| `IP_PER_MINUTE` | `20` | Max requests per IP per minute (catches players faking new IDs). |
| `PLAYER_PER_DAY` | `200` | Max requests per player per UTC day. |
| `GLOBAL_PER_DAY` | `1000` | Max requests in total per UTC day. This is your spending cap. |

Limits are counted by one Durable Object, which works on Cloudflare's free plan. Players who hit a limit see a short in-game message such as "Slow down!".

The proxy also enforces its own rules no matter what persona is sent: 1-2 sentences, plain ASCII (GD's fonts can't draw anything else), playful rather than hateful, and replies trimmed to 220 characters. If the model declines a message, the API retries on Anthropic's recommended fallback model (`fallbacks: "default"`). If that also declines, the bubble shows a neutral in-character line.

### Costs (rough)

Each allowed request is one model call, with about 600-1,000 input tokens and 100-300 output tokens (thinking included).

| Model | Per message | At the default 1,000/day cap |
|---|---|---|
| `claude-opus-5-5` ($4 / $20 per M tokens) | ~$0.005-0.01 | up to ~$10/day |
| `claude-haiku-5-5` ($0.10 / $0.50 per M tokens) | ~$0.0002 | up to ~$0.25/day |

The proxy logs real token usage for every request: `npx wrangler tail` shows it live, and the Cloudflare dashboard keeps the logs. Also set a monthly spend limit in the Claude Console as a backstop.

### Why there's no secret token in the mod

Anything compiled into a public mod can be pulled out of it, so a shared secret would only look like protection. What actually bounds abuse: input size limits, the fixed system rules, short replies, the per-player, per-IP and daily limits, and (optionally) `ALLOWED_LEVEL_IDS`.

### Local development

```sh
cp .dev.vars.example .dev.vars   # put your key in it
npm run dev                      # http://localhost:8787/chat
npm run typecheck
```

---

## 2. Build the mod

### Option A: GitHub Actions (easiest, no local toolchain)

Pushing changes under `gd-ai-rival/mod/` runs `.github/workflows/build-ai-rival-mod.yml`, which builds the mod with Geode's official action. Download the **AI Rival mod** artifact from the workflow run: it contains one `marun1105.ai-rival.geode` covering every platform that was built. You can also start it by hand from the Actions tab ("Run workflow").

> **Windows build and the repo name.** GitHub's Windows runners check out into `D:\a\<repo>\<repo>`, and Windows can't create a folder whose name ends in a dot. While the repository is called `my-project.`, the workflow therefore builds only **Android32 + Android64** and skips Windows. Rename the repo (GitHub → Settings → Repository name, e.g. `my-project`; old links redirect) and the Windows build switches on by itself, with no workflow change needed. Until then, build the Windows version locally (Option B).

### Option B: build locally

1. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli), then the SDK and its prebuilt binaries:
   ```sh
   geode sdk install
   geode sdk install-binaries
   ```
2. Install a compiler:
   - **Windows:** Visual Studio 2022 with "Desktop development with C++" (the mod uses C++23), plus CMake 3.21+.
   - **Android:** the Android NDK (r27 or newer) with `ANDROID_NDK_ROOT` set, or passed with `geode build --ndk <path>`. You can build for Android from Windows, macOS or Linux.
3. Build:
   ```sh
   cd gd-ai-rival/mod
   geode build                  # Windows
   geode build -p android64     # Android (64-bit); android32 for older phones
   ```
   The output is `build/marun1105.ai-rival.geode`. See `geode build --help` for more options.

The mod targets **GD 2.2081 with Geode v5.11.2** (`mod.json` → `"geode"`). If you install a newer Geode SDK, bump that field to match. To publish under your own name, change `id` and `developer` in `mod.json`.

---

## 3. Install

`geode build` installs the mod automatically if your CLI profile points at your GD install. Otherwise copy the `.geode` file into Geode's mods folder:

- **Windows (Steam):** right-click Geometry Dash, then Manage, then Browse local files. Put the file in `geode/mods/`.
- **Android (Geode launcher):** `Android/media/com.geode.launcher/game/geode/mods/` on your phone's storage.

The mod depends on **Node IDs** (`geode.node-ids`), which most Geode installs already have. If it's missing, Geode will tell you, and you can install it from the in-game mod list.

---

## 4. Configure in game

Geode menu → AI Rival → Settings:

| Setting | Default | What it does |
|---|---|---|
| Proxy URL | empty | Your Worker URL ending in **`/chat`**. Required. |
| AI Name | Rival | Name shown on the speech bubble. |
| Personality | rival trash-talker | Character description sent to the proxy (max 600 characters). |
| Memory Size | 10 | Recent messages sent with each request. Resets per level. |
| Open Chat (PC only) | `T` | Pauses the level and opens the chat. |
| In-Game Chat Button | on for Android, off for PC | Small chat button next to the pause button. |
| Auto Reactions | on | AI comments on deaths, new bests and completions. |
| Greet On Level Start | on | AI says something when you enter a level. |
| Death Comment Cooldown | 20 s | Minimum gap between comments on ordinary deaths. |
| Death Comment Chance | 40 % | Chance to comment on an ordinary death once the cooldown is over. |
| React In Practice Mode | off | Also react in practice mode and from start positions. |
| Bubble Position / Duration | Top / 4 s | Where the bubble sits and how long it stays (longer replies stay longer). |
| Only On Specific Levels + Level IDs | off | Client-side level lock. Unpublished editor levels have ID `0`. |

### How reactions are rate-limited

- At most one request is in flight at a time. A death comment never interrupts a chat reply.
- **Ordinary deaths:** only after the cooldown, and then only with the configured chance. Deaths that got skipped are summarized in the next comment ("other deaths since your last comment: 12%, 34%...").
- **Close calls** (within 3% of your best, once your best is at least 30%): half the cooldown, no dice roll.
- **New bests:** always commented on, with a small minimum gap so a burst of tiny bests on a fresh level doesn't spam.
- **Level start and completion:** always.
- The proxy's per-player limit applies on top of all of this.

---

## Troubleshooting

| You see | Fix |
|---|---|
| "No proxy URL set" | Fill in Proxy URL in the mod settings. |
| "Couldn't reach the AI (HTTP 404)" | The URL is missing `/chat`. |
| "The AI proxy is misconfigured." | The API key secret is missing or invalid. Run `npx wrangler secret put ANTHROPIC_API_KEY`, then check `npx wrangler tail`. |
| "The AI isn't available on this level." | The level isn't in the proxy's `ALLOWED_LEVEL_IDS`. |
| No reactions on deaths | Auto Reactions is off, you're in practice mode, the cooldown or chance skipped it, or the level lock excludes the level. |
| Replies never arrive | Open the Geode log console (Geode settings) and look for `Proxy request failed`. |

Notes: replies are English-only by design, since GD's built-in fonts can't draw non-ASCII text. Memory is per level and resets when you leave it.
