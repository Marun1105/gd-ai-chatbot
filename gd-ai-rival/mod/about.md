# AI Rival

Talk to an AI while you play. It watches your run and reacts to what happens:

- **Chat**: pause and talk to it. On PC press <cy>T</c>, or use the chat button in the pause menu (phones also get one next to the pause button).
- **Reactions**: it comments on deaths, new bests and completions, without spamming every attempt.
- **Memory**: it remembers the recent conversation for the current level.
- **Personalities**: Rival, Coach, Chill, Sarcastic, or write your own.
- **Grin**: a creepy character that lives in your screen. Every death makes it angrier (1% on a first try barely counts, 90% on an extreme demon counts a lot), and when its anger bar fills up it smashes your screen. Then fight it in a short mini-game: beat it and it calms down. Pick it under Personality.
- **Your style**, designed with live previews: drag the speech bubble where you want it on a mini game screen, pick its font, colors and talking sound, pick or draw the AI's icon, and theme the chat window.
- **Level creators** can give their level its own AI character (see below).
- **Any AI**: Claude, ChatGPT, Gemini, Grok, Groq, OpenRouter, Ollama, or any OpenAI-compatible API.

## Setup (about 2 minutes)

1. Open this mod's settings and press **Set Up** next to *AI Connection*. In a level, you can also pause, tap the chat button, then the **gear**.
2. Use the arrows to pick a provider. **FREE** ones need no payment.
3. Press **Get Key**, create a key on the website and copy it.
4. Back in the game press **Paste**, then **Test**. If you see a reply, you're done!

Tap **List** next to the model to see every model your key can use.

## Which AI should I pick?

- <cg>Free, no card needed:</c> **Gemini**, **Groq**, **OpenRouter** (free models)
- <cg>Free, runs on your own PC:</c> **Ollama** (PC only, not phones)
- <co>Paid, best replies:</c> **Claude**, **ChatGPT**, **Grok**

## Getting a key

### Gemini (Google), free
1. Go to **aistudio.google.com/apikey** and sign in with a Google account.
2. Press **Create API key** and copy it.

The free tier allows a limited number of requests per minute, which is plenty with the default reaction settings.

### Groq, free
1. Go to **console.groq.com/keys** and sign up. No card needed.
2. Press **Create API Key** and copy it.

Groq (with a q) is a different company from Grok.

### OpenRouter, free models
1. Go to **openrouter.ai/keys**, sign up and press **Create Key**.
2. Keep the model as `openrouter/free` to only use free models. Free models change over time and can be slower when busy.

### Ollama, free on your PC
1. Download and install it from **ollama.com/download**.
2. Open a terminal and run `ollama pull llama3.2`.
3. Keep Ollama running while you play. No key needed.

### Claude (Anthropic), paid
1. Go to **console.anthropic.com**, sign up and add a few dollars of credit under **Billing**.
2. Open **API Keys**, press **Create Key** and copy it (it starts with `sk-ant-`).

The default model, Claude Sonnet 5.5, costs about $0.003 per message.

### ChatGPT (OpenAI), paid
1. Go to **platform.openai.com**, sign up and add billing.
2. Open **platform.openai.com/api-keys** and press **Create new secret key**.

A ChatGPT Plus subscription does not include API credit; the API is billed separately.

### Grok (xAI), paid
1. Go to **console.x.ai**, sign up and add credits. New accounts sometimes get trial credits.
2. Open **API Keys**, create one and copy it.

### Creator proxy
Some level creators run their own server so players don't need a key at all. Pick **Creator proxy** and paste the URL the creator shared (it ends in `/chat`).

## For level creators: give your level its own AI

Put **text objects** in your level whose text starts with `ai:`. Hide them anywhere (behind a block, off screen, or invisible); the mod reads them when the level loads. Each text object can hold one setting, or several separated by `|`.

Example:

- `ai: name = Rocky | avatar = 35 | avatar-colors = 9, 12`
- `ai: persona = You are Rocky, a grumpy wizard who guards this tower. You hate the ship part.`
- `ai: on-death = If they die between 50 and 70 percent, mock the ship part.`
- `ai: on-complete = Congratulate them and tell them the secret password is TOWER.`
- `ai: bubble-color = 40, 0, 60, 200 | text-color = 255, 230, 255 | sound = chirp`

**Character**
- `name`: the AI's name
- `persona`: who it is and how it talks (up to 1000 characters)
- `preset`: `rival`, `coach`, `chill` or `sarcastic`
- `length`: `very short`, `short` or `medium`

**Reactions**
- `start`, `death`, `close-call`, `new-best`, `complete`: `off` to turn that reaction off
- `on-start`, `on-death`, `on-new-best`, `on-complete`: a direction for that moment (up to 200 characters)
- `close-call-range`: how many percent from the best counts as a close call

**Look**
- `bubble-position`: `top`, `bottom`, `top left`, `top right`, `bottom left`, `bottom right`
- `bubble-x`, `bubble-y`: nudge the bubble
- `bubble-color` (`r, g, b, alpha`), `text-color`, `name-color` (`r, g, b`, or `#rrggbb`)
- `font`: `chat`, `big` or `gold`; `text-size`; `width`; `duration` (seconds); `typewriter`: `on` or `off`
- `avatar`: a cube icon number, `player` (the player's own cube) or `off`; `avatar-colors`: two GD color numbers, plus an optional third for a glow
- `avatar-drawing`: a drawn icon. Draw it in the mod's avatar editor (Settings, Icon, Edit, Drawing), press **Copy** under Code and paste the code here
- `sound`: `blip`, `chirp`, `soft` or `off`
- `chat-color`: color of the chat window

Players can turn level customization off in the settings. A level can never change a player's AI connection, key, daily limit or comment cooldown.

## Your key and your costs

- Your key is stored only on this device and is only sent to the AI you picked. The mod hides it on screen, but don't share it.
- **Daily Message Limit** (default 200) caps how many messages the mod sends per day.
- Comments on ordinary deaths have a cooldown and a chance setting, so it doesn't use a message on every death.
