# ColorSynth
 MIDI-Controlled LEDs

> This is a functional starting point for my grand scheme of translating music into light.

> Currently exists as one giant sketch, but working on modularizing into separate bits of functionality

>Ultimately I see different note-to-color mappings packaged into selectable modules akin to a drum machine or sampler

>Implementing DMX is another eventuality, to get out of the realm of writing to wires for specific LED strip configurations. Then it splits into a bit of a encoder/decoder situation and becomes more useful for a broader range of light setups without reworking the whole thing every time

>I settled on solutions that I found pretty interesting when passing classical piano pieces or one-hit drums to get it up an running as an example

>The two big questions...
	>>What exactly is the/are some useful functions to translate from pitch/note value into color value?
	>>How to repesent interval and timbre texture


>The most obvious solution of mapping directly from pitch/note value to light wavlength/hue is fairly useless because our perception of wavelength differences in pitch and light are not similiar. C1 and C2 are similar and dissimilar in ways that red and very slightly more orangish red are not. This lead me to focusing on an octave at a time, and figuring some way to differentiate higher/lower octaves. My solution of desaturating was not the greatest, but was also not the biggest challenge.

>The are many ways you can map the 12 semitones in an octave to hue values, and one note at a time they can be interesting.
	My first voyage was informed by discovering the Scriabin mapping, which is basically progressing hues along a circle of fifths. I did not find this particularly effective. I do not experience synesthesia in this way, and these color values did not impart on me any sense of relation to the pitches of the notes.
This might be more useful if you use those hues as the general color of a piece in that key.
	I settled on a rather elementary solution of mapping half-step intervals directly to hue angles.(360/11, since 360 == 0, and there must be some gap between B1 and C2). In melodic progressions I did find a sense of ascending and descending relation between hue and note value. However, this lead to the still greater problem, which is...

>Everything turns white, pretty much instantly. The characteristics of timbre, chords, dissonance, etc.. cannot be represented by summing color values. So then there is another journey ahead



