# Novation Peak / Summit factory wavetables

List, description and usage (provided by the user, 2026-11-15; third-party descriptions).
Wavetable No. n is NRPN value `n + 3` of Osc Wave More (4–63). The order is the manual's table read down its
four columns; `src/core/ParamTable.cpp` (`kWavetables`) uses it.

| No. | Name | Description | Texture / character | Usage |
|---|---|---|---|---|
| 1 | BS Sine | Evolves from a soft organ-like tone to a harsh square-like wave | Soft, bassy | Organs, leads, keys, pads |
| 2 | Random | Random harmonics that provide a harsh and dissonant tone. PPG-like | Harsh, tinny | Noise, SFX, metal-like leads |
| 3 | Zing | Sounds almost like FM noise. Very harsh and bass-like | Harsh, buzzy | Noise, SFX, bass, lead |
| 4 | Tubey | Changes between bell and vocal-like waves. The end waves are bellish, and the middle waves are vox-like | Soft and harsh, mid-range, crystalline | Pads, keys, bells, choirs |
| 5 | Octaves | The first two harmonics shift up an octave for the next wave | Soft, additive, basic | Pads, bells, organs |
| 6 | Wobbler | Goes from a square wave to FM noise. Sounds like a cross between "Zing" and "BS Sine" | Soft and harsh, buzzy | Leads, SFX |
| 7 | Chords | Crystalline and glass tones that change chords throughout the frames. Animoog-esque | Mid-range, bright, glassy | Organs, pads, keys, maybe leads |
| 8 | Didgery | Supposed to be a didgeridoo, but it sounds much more like the PPG vox sound. Cycling it rapidly creates an "Eee-ooo" sound | Mid-range, buzzy | Pads, choirs, SFX |
| 9 | Harsh | Goes from a square-like sound to a harsh wave and then a bell-like sound. Very reminiscent of Casio's "vowel consonant" and SD keyboards | Harsh, low or mid-range, buzzy, glassy | Keys, bells, pads, leads |
| 10 | Organ | Sounds like what it's supposed to be. It goes from a soft to a harsh tonewheel organ tone | Soft to harsh, mid or high-range, additive | Organs, pads |
| 11 | E.Piano | Sounds nothing like the DX7 classic. Maybe more like a Wurlitzer. Also sounds a bit vocal-like | Harsh, cheapy, buzzy | Keys, choirs, leads |
| 12 | VoxOooEe | It has a sort of chipmunk-like vibe. It does exactly as the name suggests. Also PPG-like | Mid-range, buzzy, formant-like | Choirs, leads |
| 13 | VoxYahEe | Sounds exactly like the name suggests. It's cool what you can achieve with 5 frames! | Mid-range, buzzy, formant-like | Choirs, leads |
| 14 | Winds | Sounds like a collection of waves based on different wind instruments. The last couple of waves sound voxy | Mid-range, buzzy, soft | Leads, pads |
| 15 | SoftClav | It's supposed to sound like a clavinet, but it's still vocal or FM-like without the proper sweeping | Mid-range to thin, buzzy, harsh | Keys, leads, choirs |
| 16 | String | I guess it's supposed to be like a bowed string. Still very formant-like | Mid-range to thin, buzzy | Leads, pads, choirs |
| 17 | BassOrgan | Another organ wavetable. This one is more buzzy than the Organ one, and it has a bell waveform in the middle | Bass to mid-range, buzzy, soft | Organs, bells, keys, pads |
| 18 | Acid | Sounds very squeezy, but it does not sound like resonance | Mid-range to thin, formant-like, alien | Leads, basses, SFX, choirs |
| 19 | Buzzy | This one goes from vocal to bright | Mid-range to thin, buzzy | Choirs, leads |
| 20 | Carousel | Goes from soft to glassy. Appears to be an approximation of a Shepard tone | Bassy to bright, additive, soft | Pads, SFX |
| 21 | Choral | A vocal wavetable that is kind of chord-like | Mid-range to thin, formant, buzzy, bright | Choirs |
| 22 | Climbing | Starts as a sawtooth wave before becoming chord-like sounds | Bassy to thin, bright, shimmering | SFX, pads, keys |
| 23 | CoinFlip | Analog-like wavetable phasing from harsh to soft | Mid-range to thin, bright, soft | Leads, bass, pads |
| 24 | Deep | A muffled formant-like sound. It's like a submerged choir | Mid-range, formant-like, hollow | Choirs, pads |
| 25 | Dub | Goes from a soft square-like tone to a harsh EPiano-like waveform | Bassy to bright, soft, rich, glassy | Keys, leads, bass |
| 26 | Eee | Yet another formant wavetable | Mid-range to thin, buzzy, alien | Choirs |
| 27 | Eris | An interesting analog-like sound. It sounds sort of like a phased and harmonized saw wave. Animoog-esque | Bassy to thin, buzzy, shimmering, bright | Leads, SFX, keys |
| 28 | Flame | Very interesting, distinct, and kind of like water swishing | Thin, alien, additive | SFX, leads, pads |
| 29 | Further | A soft tone that "whistles" like throat singing | Thin, additive, dynamic | SFX, pads |
| 30 | GlassSaw | Bell-like tone that becomes a muffled formant | Mid-range, glassy, brittle | Bells, keys, leads, pads |
| 31 | Glassy | A bright, glassy sound that becomes soft and piano-like | Bright to mid-range, harmonic | Keys, organs, leads, pads |
| 32 | Granular | Chime-like sound that becomes somewhat vocal | Mid-range to bright, chimey, bright | Bells, keys, vocals, leads |
| 33 | Grime | Interesting glassy sound that is organ-like | Mid-range, bright, organ-like, bellish, shimmering | Keys, organs, bells, pads |
| 34 | Grow | Kind of a reverse version of "Glassy". It's a piano sound that starts to become bright and glassy | Mid-range, analog-like, glistening, chimey | Keys, bells, pads |
| 35 | Heavy | A mixture of analog and FM/formant-like sounds | Mid-range, basic | Plucks, bass, keys, leads |
| 36 | Hedge | Yet another glassy formant wavetable | Mid-range to thin, alien or formant-like | Choirs, leads |
| 37 | Hungry | Evolves from an EPiano-like sound to a PPG bellish sound | Bass to thin, chimey, bright | Keys, bells |
| 38 | Ladders | Switches between formant and chime-like | Mid-range, buzzy, mellow | Keys, choirs, pads |
| 39 | Lead | Sounds more digital than analog. Very vocal-like | Mid-range to thin, buzzy, formant-like | Leads, choirs |
| 40 | Modeling | Is this supposed to be based on an analog or acoustic sound? It's almost completely bell or chime-like | Mid-range to bright, glassy, chimey, bell-like, simple | Bells, keys, pads |
| 41 | Modem | Another bell-like wavetable that is pleasant and fun to use! | Mid-range to bright, glassy, chimey, bell-like, simple | Bells, keys, pads |
| 42 | Monster | Interesting square or FM-like wavetable | Bassy to mid-range, hollow, smooth, mellow, odd harmonics | Keys, bells, pads, leads, bass/pluck |
| 43 | Screech | Thin sound that becomes glassy | Thin, bright, harmonic, alien | Leads, SFX |
| 44 | SeaBase | Interesting mixture of chime, formant, and organ-like waves. The first wave stands out to me the most, and it is very bell-like | Bassy to mid-range, chimey, formant, organ-like, buzzy | Pads, keys, bells, choirs, SFX |
| 45 | Shmorgan | Sounds more bell than organ-like | Bassy to mid-range, soft, mellow, bright, chimey | Bells, keys/organs, pads, leads |
| 46 | Spirals | Saw-like sound that instantly becomes a series of dissonant chords | Thin, bright, sweepy, chord-like | SFX |
| 47 | Steels | Soft square-like tone that becomes an in-between chime | Bassy to mid-range, hollow, glassy, odd harmonics | Keys, bells, leads, bass |
| 48 | Sunrise | Shimmering organ-like wavetable | Mid-range to thin, shimmering, bright, chimey, organ-like | Organs, keys |
| 49 | Swell | Quite thin and glassy. Seems useless or redundant to me | Thin, organ-like, chimey, bright, harsh | SFX |
| 50 | Thicker | Piano-like sound that becomes glassy | Bassy to mid-range, hollow, mellow, glassy, bright | Keys, pads, bells |
| 51 | Thinner | Goes from a bell to a steel-like sound. Very glassy and metallic | Mid-range to thin, bright, harsh, glassy | Bells, SFX |
| 52 | Tides | Square-like tone that starts to sound like an EPiano, and then a chimey organ | Mid-range to thin, hollow, chimey, bright | SFX, keys, bells |
| 53 | Tokyo | A combination of bright, PPG or organ-like waves | Mid-range to thin, bright, harsh, glassy | SFX, keys, organs, bells |
| 54 | Tops | Goes from a thin to a full-sounding bell wave. Might be useful for percussive sounds | Thin to mid-range, bright, chimey | SFX, bells, keys, pads |
| 55 | V.Chord | Excellent and unique wavetable! It's a set of chords with variable analog-like waves. Also chiptuney | Mid-range, analog-like, bell-like, bright | Keys, leads, pads |
| 56 | Variance | Nice set of bell-like waveforms! | Mid-range to thin, chimey, bright, glassy | Bells, keys, pads, organs, leads |
| 57 | Vocaloid | More PPG than Miku-like | Thin to mid-range, formant, alien | Choirs, leads, pads |
| 58 | Vowelled | EPiano-like wavetable with vocal undertones | Bassy to mid-range, mellow, hollow, soft | Keys, pads, choirs |
| 59 | WeirdVox | Thin and glassy vocal-like wavetable. Sounds more bell than vox-like | Mid-range to thin, formant, alien, chimey | Choirs, keys, pads, leads, SFX |
| 60 | Yeah | Sounds like what the name suggests! | Mid-range, formant, alien | Choirs, leads |
