# AI Rival: talk to an AI while you play Geometry Dash

A Geode mod for Geometry Dash 2.2091 (Windows and Android) that puts an AI in your levels. You can chat with it, and it reacts on its own to your deaths, new bests and completions. Replies show up in a speech bubble that fades out after a few seconds.

Each player connects their **own AI**: Claude, ChatGPT, Gemini, Grok, Groq, OpenRouter, Ollama, or any OpenAI-compatible API. Several of these are free. Level creators can instead run a **proxy** that pays for the AI, so their players don't need a key.

```
Own key:        GD + mod  --HTTPS + player's key-->  Claude / OpenAI / Gemini / ...
Creator proxy:  GD + mod  --HTTPS-->  Cloudflare Worker (holds the creator's key)  -->  Claude
```

| Folder | What it is |
|---|---|
| `mod/` | Geode mod (C++23, Geode SDK v6) |
| `proxy/` | Optional Cloudflare Worker for creators who pay for the AI themselves |
| `../.github/workflows/build-ai-rival-mod.yml` | GitHub Actions build for Windows + Android |

---

## 1. Build the mod

### Option A: GitHub Actions (easiest, no local toolchain)

Pushing changes under `gd-ai-rival/mod/` runs `.github/workflows/build-ai-rival-mod.yml`, which builds the mod with Geode's official action. Download the **AI Rival mod** artifact from the workflow run: it contains one `marun1105.ai-rival.geode` covering every platform that was built. To rebuild without a code change, open a run in the Actions tab and press **Re-run all jobs**.

> **Windows build and the repo name.** GitHub's Windows runners check out into `D:\a\<repo>\<repo>`, and Windows can't create a folder whose name ends in a dot. While the repository is called `my-project.`, the workflow builds only **Android32 + Android64** and skips Windows. Rename the repo (GitHub → Settings → Repository name, e.g. `my-project`; old links redirect) and the Windows build switches on by itself.

### Option B: build locally

1. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli), then the SDK and its prebuilt binaries:
   ```sh
   geode sdk install
   geode sdk install-binaries
   ```
2. Install a compiler:
   - **Windows:** Visual Studio 2022 with "Desktop development with C++" (the mod uses C++23), plus CMake 3.21+.
   - **Android:** the Android NDK (r27 or newer) with `ANDROID_NDK_ROOT` set, or passed with `geode build --ndk <path>`.
3. Build:
   ```sh
   cd gd-ai-rival/mod
   geode build                  # Windows
   geode build -p android64     # Android (64-bit); android32 for older phones
   ```
   The output is `build/marun1105.ai-rival.geode`.

The mod targets **GD 2.2091 with Geode v6.0.0-alpha.1** (`mod.json` → `"geode"`). If you install a newer Geode SDK, bump that field to match. To publish under your own name, change `id` and `developer` in `mod.json`.

---

## 2. Install

`geode build` installs the mod automatically if your CLI profile points at your GD install. Otherwise copy the `.geode` file into Geode's mods folder:

- **Windows (Steam):** right-click Geometry Dash → Manage → Browse local files, then put the file in `geode/mods/`.
- **Android (Geode launcher):** `Android/media/com.geode.launcher/game/geode/mods/` on your phone's storage.

The mod has no other mod dependencies. If **Node IDs** is installed, the pause-menu chat button joins its shared button column; otherwise it sits on its own on the right.

---

## 3. Connect an AI

Open the mod's settings and press **Set Up** next to *AI Connection* (or, in a level: pause → chat button → gear).

1. Use the arrows to pick a provider. Each one shows **FREE** or **PAID** and a short note.
2. Press **Get Key**, create a key on the provider's site and copy it.
3. Press **Paste**, then **Test**. A reply in green means it works.
4. Optional: press **List** to pick another model from the provider's live model list.

| Provider | Cost | Default model |
|---|---|---|
| Claude (Anthropic) | Paid, ~$0.003/message | `claude-sonnet-5-5` |
| ChatGPT (OpenAI) | Paid | `gpt-5.6` |
| Gemini (Google) | **Free tier** (rate limited) | `gemini-flash-latest` |
| Grok (xAI) | Paid (trial credits sometimes) | `grok-4.5` |
| Groq | **Free tier**, no card | `llama-3.3-70b-versatile` |
| OpenRouter | **Free models** available | `openrouter/free` |
| Ollama (your PC) | **Free**, offline, PC only | `llama3.2` |
| Custom (OpenAI-compatible) | Depends | you enter it |
| Creator proxy | Free for players | set by the proxy |

The step-by-step guide for every provider is in [`mod/about.md`](mod/about.md), which is also the mod's page in-game. Model names change often, which is why **List** exists: if a default stops working, pick a current model from the list.

Keys are saved per provider in the mod's save data on the device, shown masked on screen, never logged, and only sent to the provider they belong to.

---

## 4. Settings

Everything is in Geode → AI Rival → Settings, grouped into sections:

| Section | What you can change |
|---|---|
| Connection | AI Connection screen, Daily Message Limit (default 200, `0` = none) |
| Character | Name, Personality (Rival, Coach, Chill, Sarcastic, Grin, Custom), custom personality text (big editor with paste and examples, up to 1000 characters), Reply Length (Very Short / Short / Medium), Memory Size |
| Grin | Anger bar (fills with every death, weighted by how far you got, the level's difficulty and repeat death spots; calms on new bests), Temper (Patient / Normal / Short Fuse), Screen Break (when it snaps: darkness, a roar, and your screen cracks and shatters), Roar Volume |
| Controls | Open Chat keybind (PC, default `T`), In-Game Chat Button (on for phones) |
| Game Reactions | Master switch, each reaction on or off (start, deaths, close calls, new bests, completion), close call range, death cooldown and chance, practice mode, and a custom direction for each moment ("give me a tip instead of roasting me") |
| Speech Bubble | **Bubble Designer** with a live preview: drag the bubble around a mini game screen (it snaps to the nearest of 6 spots), width, font, text size, how long it stays, letter-by-letter text, color themes or any colors with transparency, talking sound and volume, and a close-up that replays the reply with its voice |
| Icon | Avatar editor with a live bubble preview: pick a cube, colors and glow from the icon kit, draw your own pixel icon with 8 to 32 squares per side (pen, fill, mirror, undo, shareable code), use your own icon, or none |
| Chat Window | Theme (classic GD or a color from presets or a picker) and size, with a live copy of the window |
| Levels | Let levels customize the AI, and the optional level lock |

### For level creators

A level can give the AI its own name, personality, look, sound and reactions with hidden **text objects** whose text starts with `ai:`, for example `ai: name = Rocky | avatar = 35 | sound = chirp` or `ai: on-death = mock the ship part`. The full list of keys is in [`mod/about.md`](mod/about.md#for-level-creators-give-your-level-its-own-ai) (also shown on the mod's page in-game). A level can't change a player's AI connection, key, daily limit or cooldowns, and players can turn level customization off.

### How reactions are rate-limited

- At most one request is in flight at a time. A death comment never interrupts a chat reply.
- **Ordinary deaths:** only after the cooldown, and then only with the configured chance. Skipped deaths are summarized in the next comment ("other deaths since your last comment: 12%, 34%...").
- **Close calls** (within 3% of your best, once your best is at least 30%): half the cooldown, no dice roll.
- **New bests:** always commented on (if on), at most one every 8 seconds.
- **Level start and completion:** always.
- The **Daily Message Limit** caps everything, chat included.

Every reply follows fixed rules whichever AI is used: the chosen reply length, plain ASCII (GD's fonts can't draw anything else), playful rather than hateful. Replies are cleaned up and trimmed (120 / 220 / 320 characters for Very Short / Short / Medium) before they're shown.

---

## 5. Optional: run a creator proxy

If you want players of **your** level to use the AI without their own key, deploy the Cloudflare Worker in `proxy/` with your key. Players pick **Creator proxy** in AI Connection and paste your URL. You pay for their messages, so the proxy enforces per-player, per-IP and daily limits.

You need Node.js 18+, a free [Cloudflare account](https://dash.cloudflare.com/sign-up) and a key from the [Claude Console](https://console.anthropic.com/).

```sh
cd gd-ai-rival/proxy
npm install
npx wrangler login                         # opens the browser once
npx wrangler secret put ANTHROPIC_API_KEY  # paste your key when asked (CLAUDE_API_KEY works too)
npx wrangler deploy
```

Your URL is printed by `deploy`; players need it **plus `/chat`**, e.g. `https://gd-ai-rival.<you>.workers.dev/chat`.

Edit `vars` in `proxy/wrangler.jsonc`, then deploy again:

| Variable | Default | Meaning |
|---|---|---|
| `MODEL` | `claude-opus-5-5` | Claude model. `claude-haiku-5-5` is much cheaper. |
| `ALLOWED_LEVEL_IDS` | empty | Level IDs the proxy answers for. Empty = all. Set this to your level so strangers can't use your proxy on other levels. |
| `ALLOW_CUSTOM_PERSONA` | `true` | `false` ignores the player's personality and always uses `DEFAULT_PERSONA`. |
| `DEFAULT_PERSONA` | the rival | Character used when custom personas are off or empty. |
| `PLAYER_PER_MINUTE` | `8` | Max requests per player per minute. |
| `IP_PER_MINUTE` | `20` | Max requests per IP per minute. |
| `PLAYER_PER_DAY` | `200` | Max requests per player per UTC day. |
| `GLOBAL_PER_DAY` | `1000` | Max requests in total per UTC day (your spending cap). |

Rough proxy costs: `claude-opus-5-5` ~$0.005-0.01 per message (up to ~$10/day at the 1,000 cap); `claude-haiku-5-5` ~$0.0002 per message. `npx wrangler tail` shows live token usage. Also set a monthly spend limit in the Claude Console.

Local development: `cp .dev.vars.example .dev.vars`, add your key, then `npm run dev` (serves `http://localhost:8787/chat`) and `npm run typecheck`.

---

## Troubleshooting

| You see | Fix |
|---|---|
| "isn't connected to an AI yet" | Open AI Connection (gear in the chat) and add a key. |
| "... rejected the API key" | The key is wrong or was deleted. Create a new one and paste it again. |
| "... doesn't know the model ..." | Press **List** in AI Connection and pick a current model. |
| "too many requests, or you're out of credits" | Free tiers have per-minute limits; wait a bit. On paid providers, add credit. |
| "Couldn't reach Ollama" | Start Ollama on your PC and run `ollama pull llama3.2` once. |
| "Daily message limit reached" | Raise Daily Message Limit in the settings, or wait until tomorrow. |
| "Proxy not found" | The creator proxy URL must end in `/chat`. |
| No reactions on deaths | Auto Reactions is off, you're in practice mode, the cooldown or chance skipped it, or the level lock excludes the level. |

Notes: replies are English-only by design, since GD's built-in fonts can't draw non-ASCII text. Memory is per level and resets when you leave it.
