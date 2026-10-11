"""Prepares the DARK BLOOD sound bank (Phase 18, docs/AUDIO.md) from CC0 source packs into SourceArt/Audio.

    python -I Tools/Audio/db_prepare_audio.py <kenney_dir> <opengameart_dir>

Sources (all CC0, see docs/CREDITS.md): Kenney "RPG Audio", "Impact Sounds", "Interface Sounds"; OpenGameArt
"Swishes Sound Pack" and "RPG Sound Pack" (artisticdude), "Crickets Ambient Noise" (Wolfgang_), "Birds and Wind -
Ambient" (Spring Spring), "Sea and river wave sounds" (RandomMind), "Asianoriental1" (Tozan), "Determined Pursuit" and
"QaziJamJam" (Emma_MA). Waterfall roar, mountain wind and the drone of the demon lands are synthesized here.

Every file is resampled to 48 kHz 16 bit. One-shots: silence trimmed, mono, peak -3 dBFS. Loops (ambience, music):
stereo, the end crossfaded into the start so they loop without a seam, loudness matched by RMS. Needs numpy, soundfile.
"""
import glob
import os
import sys

import numpy as np
import soundfile as sf

RATE = 48000
PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(PROJECT, "SourceArt", "Audio")


def load(path):
    data, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        # Linear resampling is plenty for these sources (all >= 44.1 kHz); keeps the script dependency-light.
        count = int(round(data.shape[0] * RATE / rate))
        positions = np.linspace(0.0, data.shape[0] - 1, count)
        data = np.stack([np.interp(positions, np.arange(data.shape[0]), data[:, c]) for c in range(data.shape[1])], axis=1)
    return data


def trim(data, threshold=0.004):
    level = np.abs(data).max(axis=1)
    loud = np.where(level > threshold)[0]
    if loud.size == 0:
        return data
    start = max(0, loud[0] - int(0.004 * RATE))
    end = min(data.shape[0], loud[-1] + int(0.03 * RATE))
    return data[start:end]


def peak_normalize(data, peak_db=-3.0):
    peak = np.abs(data).max()
    return data if peak <= 0 else data * (10 ** (peak_db / 20.0) / peak)


def rms_normalize(data, rms_db):
    rms = np.sqrt(np.mean(np.square(data)))
    data = data * (10 ** (rms_db / 20.0) / max(rms, 1e-9))
    peak = np.abs(data).max()
    return data * (0.97 / peak) if peak > 0.97 else data


def seamless(data, fade_seconds=2.0):
    """Crossfade the tail into the head: the result loops without a click or a jump."""
    fade = min(int(fade_seconds * RATE), data.shape[0] // 4)
    ramp = np.linspace(0.0, 1.0, fade)[:, None]
    head = data[:fade] * ramp + data[-fade:] * (1.0 - ramp)
    return np.concatenate([head, data[fade:-fade]], axis=0)


def save(category, name, data, mono=False):
    folder = os.path.join(OUT, category)
    os.makedirs(folder, exist_ok=True)
    if mono and data.shape[1] > 1:
        data = data.mean(axis=1, keepdims=True)
    elif not mono and data.shape[1] == 1:
        data = np.repeat(data, 2, axis=1)
    path = os.path.join(folder, name + ".wav")
    sf.write(path, np.clip(data, -1.0, 1.0), RATE, subtype="PCM_16")
    return path


def one_shots(category, name, files, peak_db=-3.0):
    files = sorted(files)
    for index, path in enumerate(files):
        save(category, "%s_%02d" % (name, index + 1), peak_normalize(trim(load(path)), peak_db), mono=True)
    print("%-10s %-18s %2d variations" % (category, name, len(files)))


def loop(category, name, data, rms_db, fade=2.0):
    save(category, name, rms_normalize(seamless(data, fade), rms_db))
    print("%-10s %-18s loop %.0f s" % (category, name, data.shape[0] / RATE))


# ---- Synthesis -------------------------------------------------------------------------------------------------

def noise(seconds, seed):
    return np.random.default_rng(seed).standard_normal((int(seconds * RATE), 2))


def lowpass(data, cutoff, order=2):
    """Zero-phase low-pass in the frequency domain. Circular, so synthesized noise stays seamless when looped."""
    spectrum = np.fft.rfft(data, axis=0)
    frequencies = np.fft.rfftfreq(data.shape[0], 1.0 / RATE)
    response = 1.0 / (1.0 + (frequencies / cutoff) ** (2 * order))
    return np.fft.irfft(spectrum * response[:, None], n=data.shape[0], axis=0)


def waterfall(seconds=40.0):
    """A roar: broadband noise, a deep rumbling layer and slow surges of spray."""
    body = lowpass(noise(seconds, 1), 2400.0)
    rumble = lowpass(noise(seconds, 2), 160.0) * 4.0
    hiss = noise(seconds, 3) - lowpass(noise(seconds, 3), 5000.0)
    t = np.arange(body.shape[0]) / RATE
    surge = 1.0 + 0.18 * np.sin(2 * np.pi * 0.11 * t)[:, None] + 0.1 * np.sin(2 * np.pi * 0.37 * t + 1.3)[:, None]
    return (body + rumble + 0.25 * hiss) * surge


def mountain_wind(seconds=60.0):
    """Gusts: low-passed noise whose brightness and strength drift slowly."""
    t = np.arange(int(seconds * RATE)) / RATE
    low = lowpass(noise(seconds, 4), 300.0)
    mid = lowpass(noise(seconds, 5), 900.0)
    gust = 0.55 + 0.45 * np.sin(2 * np.pi * 0.05 * t + np.sin(2 * np.pi * 0.013 * t) * 3.0)
    whistle = np.sin(2 * np.pi * (620.0 + 80.0 * np.sin(2 * np.pi * 0.07 * t)) * t) * 0.012 * gust ** 3
    return low * 1.6 + mid * gust[:, None] + whistle[:, None]


def demon_drone(seconds=60.0):
    """The demon lands: two detuned low tones beating against each other over a dark rumble."""
    t = np.arange(int(seconds * RATE)) / RATE
    swell = 0.7 + 0.3 * np.sin(2 * np.pi * 0.04 * t)
    left = np.sin(2 * np.pi * 55.0 * t) + 0.6 * np.sin(2 * np.pi * 82.6 * t) + 0.25 * np.sin(2 * np.pi * 110.3 * t)
    right = np.sin(2 * np.pi * 55.4 * t) + 0.6 * np.sin(2 * np.pi * 82.2 * t) + 0.25 * np.sin(2 * np.pi * 109.8 * t)
    tones = np.stack([left, right], axis=1) * swell[:, None] * 0.25
    return tones + lowpass(noise(seconds, 6), 120.0) * 3.0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    kenney, oga = sys.argv[1], sys.argv[2]
    k_impact = os.path.join(kenney, "kenney_impact-sounds", "Audio")
    k_rpg = os.path.join(kenney, "kenney_rpg-audio", "Audio")
    k_ui = os.path.join(kenney, "kenney_interface-sounds", "Audio")
    rpg = os.path.join(oga, "rpg_sound_pack", "RPG Sound Pack")

    def kfiles(folder, prefix):
        return glob.glob(os.path.join(folder, prefix + "*.ogg"))

    # Combat
    one_shots("Combat", "Swing", glob.glob(os.path.join(oga, "swishes", "swishes", "swish-*.wav")) + glob.glob(os.path.join(rpg, "battle", "swing*.wav")), -6.0)
    one_shots("Combat", "HitFlesh", kfiles(k_impact, "impactPunch_medium") + kfiles(k_rpg, "knifeSlice"))
    one_shots("Combat", "HitHeavy", kfiles(k_impact, "impactPunch_heavy") + kfiles(k_impact, "impactSoft_heavy"))
    one_shots("Combat", "HitClaw", kfiles(k_impact, "impactSoft_medium"))
    one_shots("Combat", "Block", kfiles(k_impact, "impactMetal_medium") + kfiles(k_impact, "impactMetal_heavy"))
    one_shots("Combat", "Parry", kfiles(k_impact, "impactBell_heavy") + [os.path.join(rpg, "inventory", "metal-ringing.wav")])
    one_shots("Combat", "Stagger", kfiles(k_impact, "impactPlate_heavy"))
    one_shots("Combat", "GroundBlast", kfiles(k_impact, "impactMining"))
    one_shots("Combat", "Unsheathe", glob.glob(os.path.join(rpg, "battle", "sword-unsheathe*.wav")), -6.0)
    # Demon voices
    one_shots("Voice", "DemonGrowl", glob.glob(os.path.join(rpg, "NPC", "shade", "*.wav")), -4.0)
    one_shots("Voice", "DemonDeath", glob.glob(os.path.join(rpg, "NPC", "gutteral beast", "*.wav")), -4.0)
    one_shots("Voice", "BossRoar", glob.glob(os.path.join(rpg, "NPC", "giant", "*.wav")) + glob.glob(os.path.join(rpg, "NPC", "ogre", "*.wav")), -3.0)
    # Footsteps by ground
    one_shots("Footsteps", "Grass", kfiles(k_impact, "footstep_grass"), -9.0)
    one_shots("Footsteps", "Stone", kfiles(k_impact, "footstep_concrete"), -9.0)
    one_shots("Footsteps", "Snow", kfiles(k_impact, "footstep_snow"), -9.0)
    one_shots("Footsteps", "Wood", kfiles(k_impact, "footstep_wood"), -9.0)
    one_shots("Footsteps", "Dirt", kfiles(k_rpg, "footstep"), -9.0)
    # Interface and items
    one_shots("UI", "Notify", kfiles(k_ui, "confirmation"), -8.0)
    one_shots("UI", "Click", kfiles(k_ui, "click"), -10.0)
    one_shots("UI", "Open", kfiles(k_ui, "open"), -10.0)
    one_shots("UI", "Close", kfiles(k_ui, "close"), -10.0)
    one_shots("UI", "Error", kfiles(k_ui, "error"), -10.0)
    one_shots("UI", "Coins", glob.glob(os.path.join(rpg, "inventory", "coin*.wav")) + kfiles(k_rpg, "handleCoins"), -8.0)
    one_shots("UI", "Equip", glob.glob(os.path.join(rpg, "inventory", "chainmail*.wav")) + [os.path.join(rpg, "inventory", "armor-light.wav")], -8.0)

    # Ambience loops (RMS dBFS: quiet beds under everything)
    loop("Ambience", "ForestDay", load(os.path.join(oga, "Birds and Wind - Ambient_1.ogg")), -26.0, 3.0)
    crickets = load(os.path.join(oga, "crickets_1.mp3"))
    loop("Ambience", "Night", np.concatenate([crickets] * 3, axis=0), -27.0, 1.0)
    river = load(os.path.join(oga, "VistulaShort.mp3"))
    loop("Ambience", "Shore", river[: 90 * RATE], -25.0, 3.0)
    loop("Ambience", "Waterfall", waterfall(), -18.0, 3.0)
    loop("Ambience", "Wind", mountain_wind(), -26.0, 4.0)
    loop("Ambience", "DemonLands", demon_drone(), -27.0, 4.0)

    # Music (RMS -20 dBFS; the tracks fade against each other in the game)
    loop("Music", "Explore", load(os.path.join(oga, "asianoriental1_0.ogg")), -21.0, 2.5)
    loop("Music", "Battle", load(os.path.join(oga, "QaziJamJam.wav")), -19.0, 2.0)
    loop("Music", "Boss", load(os.path.join(oga, "determined_pursuit_loop.wav")), -18.0, 0.05)
    print("written to", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
