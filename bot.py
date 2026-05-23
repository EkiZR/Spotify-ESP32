import discord
import asyncio
import time
import json
import re
import urllib.request
import urllib.parse
from aiohttp import web

# ════════════════════════════════════════════════════════
# CONFIG
# ════════════════════════════════════════════════════════

BOT_TOKEN = ""         # use quotes because this is a string
USER_ID =      # NO quotes, must be integer

PORT = 3000
LYRIC_LEAD = 2.4

# ════════════════════════════════════════════════════════
# STATE
# ════════════════════════════════════════════════════════

current = {}

lyrics = {
    "key": "",
    "lines": [],
    "duration": 0,
}

# ════════════════════════════════════════════════════════
# FETCH LYRICS
# ════════════════════════════════════════════════════════

def fetch_lyrics(artist, title):

    url = (
        "https://lrclib.net/api/get"
        f"?artist_name={urllib.parse.quote(artist)}"
        f"&track_name={urllib.parse.quote(title)}"
    )

    for attempt in range(3):

        try:

            req = urllib.request.Request(
                url,
                headers={
                    "User-Agent": "ESP32-Spotify/1.0"
                }
            )

            with urllib.request.urlopen(req, timeout=10) as res:
                data = json.loads(res.read())

            synced = data.get("syncedLyrics", "") or ""
            plain = data.get("plainLyrics", "") or ""
            duration = data.get("duration", 0)

            lines = []

            # synced lyrics
            if synced:

                for line in synced.split("\n"):

                    m = re.match(r'\[(\d+):(\d+\.\d+)\](.*)', line)

                    if m:

                        secs = int(m.group(1)) * 60 + float(m.group(2))
                        text = m.group(3).strip()

                        if text:
                            lines.append({
                                "time": secs,
                                "text": text
                            })

            # plain lyrics fallback
            elif plain:

                lines = [
                    {"time": 0, "text": l.strip()}
                    for l in plain.split("\n")
                    if l.strip()
                ]

            print(f"[lrclib] success ({len(lines)} lines)")
            return lines, duration

        except Exception as e:

            print(f"[lrclib] retry {attempt+1}/3 error: {e}")
            time.sleep(1)

    print("[lrclib] failed after 3 retries")
    return [], 0

# ════════════════════════════════════════════════════════
# GET CURRENT LYRIC
# ════════════════════════════════════════════════════════

def get_lyric(progress_secs):

    lines = lyrics["lines"]
    duration = lyrics["duration"]

    if not lines:

        return {
            "prev": "",
            "curr": "",
            "next": ""
        }

    # plain lyrics mode
    if lines[0]["time"] == 0:

        total = duration or 200

        i = min(
            int((progress_secs / total) * len(lines)),
            len(lines) - 1
        )

        return {
            "prev": lines[i - 1]["text"] if i > 0 else "",
            "curr": lines[i]["text"],
            "next": lines[i + 1]["text"] if i + 1 < len(lines) else "",
        }

    # synced lyrics mode
    idx = 0

    for i, line in enumerate(lines):

        if line["time"] <= progress_secs:
            idx = i
        else:
            break

    return {
        "prev": lines[idx - 1]["text"] if idx > 0 else "",
        "curr": lines[idx]["text"],
        "next": lines[idx + 1]["text"] if idx + 1 < len(lines) else "",
    }

# ════════════════════════════════════════════════════════
# DISCORD
# ════════════════════════════════════════════════════════

intents = discord.Intents.default()

intents.presences = True
intents.members = True

bot = discord.Client(intents=intents)

# ════════════════════════════════════════════════════════
# READY
# ════════════════════════════════════════════════════════

@bot.event
async def on_ready():

    print("====================================")
    print(f"[bot] logged in as {bot.user}")
    print(f"[bot] watching user ID: {USER_ID}")
    print("====================================")

    print("[server] running")
    print(f"[local] http://localhost:{PORT}/now-playing")
    print(f"[local] http://127.0.0.1:{PORT}/now-playing")
    print("====================================")

# ════════════════════════════════════════════════════════
# REALTIME SPOTIFY DETECTION
# ════════════════════════════════════════════════════════

@bot.event
async def on_presence_update(before, after):

    print("\n========== PRESENCE UPDATE ==========")

    print("AFTER ID:", after.id)
    print("USER_ID :", USER_ID)

    print("AFTER TYPE:", type(after.id))
    print("USER TYPE :", type(USER_ID))

    print("ACTIVITIES:", after.activities)

    # skip other users
    if after.id != USER_ID:

        print("[skip] not target user")
        return

    print("[target user detected]")

    spotify = next(
        (
            a for a in after.activities
            if isinstance(a, discord.Spotify)
        ),
        None
    )

    print("SPOTIFY OBJECT:", spotify)

    # spotify not found
    if not spotify:

        current.update({
            "playing": False
        })

        print("[spotify] not detected")
        return

    print("[spotify] detected")

    artist = ", ".join(spotify.artists)
    title = spotify.title

    print("TITLE :", title)
    print("ARTIST:", artist)

    key = f"{artist}|{title}"

    # fetch lyrics when new track detected
    if lyrics["key"] != key:

        print(f"[lyrics] fetching: {artist} - {title}")

        lines, duration = await asyncio.get_event_loop().run_in_executor(
            None,
            fetch_lyrics,
            artist,
            title
        )

        lyrics.update({
            "key": key,
            "lines": lines,
            "duration": duration
        })

        print(f"[lyrics] loaded {len(lines)} lines")

    # update current state
    current.update({
        "playing": True,
        "title": title,
        "artist": artist,
        "album": spotify.album,
        "start_epoch": spotify.start.timestamp(),
        "end_epoch": spotify.end.timestamp(),
        "duration_ms": int(
            (spotify.end - spotify.start).total_seconds() * 1000
        ),
        "track_id": spotify.track_id,
    })

    print("[current state updated]")

# ════════════════════════════════════════════════════════
# API
# ════════════════════════════════════════════════════════

async def handle_now_playing(request):

    if not current.get("playing"):

        return web.json_response({
            "playing": False
        })

    now = time.time()

    pos_ms = int(
        (now - current["start_epoch"]) * 1000
    )

    pos_secs = pos_ms / 1000 + LYRIC_LEAD

    lyric = get_lyric(pos_secs)

    return web.json_response({
        "playing": True,
        "title": current["title"],
        "artist": current["artist"],
        "position_ms": pos_ms,
        "duration_ms": current["duration_ms"],
        "lyric_prev": lyric["prev"],
        "lyric_curr": lyric["curr"],
        "lyric_next": lyric["next"],
    })

async def handle_debug(request):

    return web.json_response({
        "current": current,
        "lyrics": {
            "key": lyrics["key"],
            "total": len(lyrics["lines"]),
            "duration": lyrics["duration"],
            "sample": lyrics["lines"][:5],
        }
    })

# ════════════════════════════════════════════════════════
# MAIN
# ════════════════════════════════════════════════════════

async def main():

    app = web.Application()

    app.router.add_get("/now-playing", handle_now_playing)
    app.router.add_get("/debug", handle_debug)

    runner = web.AppRunner(app)

    await runner.setup()

    await web.TCPSite(
        runner,
        "0.0.0.0",
        PORT
    ).start()

    await bot.start(BOT_TOKEN)

asyncio.run(main())
