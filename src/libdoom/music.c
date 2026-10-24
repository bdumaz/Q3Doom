/*
===========================================================================
libdoom - MUS music through an OPL2-style FM synthesizer

Doom's music is MUS: a compact MIDI-like score, played at 140 ticks per
second by DMX on the OPL chip of the AdLib and Sound Blaster cards, with
the instruments of the GENMIDI lump. This plays the same score with the
same instruments on a software model of the OPL's two-operator voices:
the four OPL2 waveforms, frequency multipliers, attack / decay / sustain /
release envelopes, total level and key scaling, feedback, FM or additive
connection, tremolo and vibrato. It follows the chip's behaviour rather
than emulating it register by register, and it has more voices than the
chip, with MIDI pan for stereo (like DMX on the OPL3).

GENMIDI: "#OPL_II#", then 175 instruments of 36 bytes (128 melodic, 47
percussion for keys 35-81):
	u16 flags (1: fixed pitch, 4: two voices), u8 fine tuning of the second
	voice, u8 fixed note, then two voices of 16 bytes:
		modulator: characteristic (reg 0x20), attack/decay (0x60),
		sustain/release (0x80), waveform (0xE0), key scale (0x40 bits 6-7),
		level (0x40 bits 0-5)
		u8 feedback/connection (0xC0)
		carrier: the same six bytes
		u8 unused, s16 base note offset

MUS: "MUS\x1a", u16 score length, u16 score start, u16 primary channels,
u16 secondary channels, u16 instrument count, u16 reserved, instruments;
then events: a byte (bit 7: a delay follows, bits 4-6: type, bits 0-3:
channel, 15 is percussion) and its data; a delay is a variable-length
number of ticks (7 bits per byte, bit 7 set while more follow).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"
#include <math.h>

#define MUS_TICRATE			140
#define MUS_CHANNELS		16
#define MUS_PERCUSSION		15
#define GENMIDI_HEADER		8
#define GENMIDI_INSTRUMENTS	175
#define GENMIDI_SIZE		36
#define MAX_VOICES			32
#define SINE_SIZE			1024
#define MAX_ATTENUATION		96.0f

#define PI_F				3.14159265f

enum { EG_OFF, EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE };

typedef struct {
	uint8_t		characteristic, attack, sustain, waveform, scale, level;
} opl_op_t;

typedef struct {
	opl_op_t	mod, car;
	uint8_t		feedback;
	int			baseNote;
} opl_voicedef_t;

typedef struct {
	int				flags, fineTuning, fixedNote;
	opl_voicedef_t	voices[2];
} opl_instrument_t;

typedef struct {
	float		phase;			// cycles
	int			state;
	float		env;			// attenuation in dB
	float		attackAmp;		// linear level during the attack
	float		out1, out2;		// last outputs, for the feedback
} opl_opstate_t;

typedef struct {
	int						active;
	int						channel, key, age;
	const opl_instrument_t	*instrument;
	const opl_voicedef_t	*def;
	int						second;			// the second voice of a double-voice instrument
	float					note;			// semitones, before the pitch bend
	float					freq;			// Hz
	float					keyAtten;		// key scale attenuation, dB
	int						rateShift;		// key scale of the envelope rates
	float					noteVolume;
	opl_opstate_t			op[2];			// modulator, carrier
} opl_voice_t;

typedef struct {
	int		instrument;
	float	volume, expression, pan;		// 0-1
	float	bend;							// semitones
	int		noteVolume;
} mus_channel_t;

struct doom_music_s {
	int					rate;
	int					loop;
	const uint8_t		*score;
	int					scoreLen, pos;
	float				samplesToEvent;
	int					done;

	opl_instrument_t	instruments[GENMIDI_INSTRUMENTS];
	mus_channel_t		channels[MUS_CHANNELS];
	opl_voice_t			voices[MAX_VOICES];
	int					age;
	float				lfoPhase;			// tremolo and vibrato, seconds
};

static float	sineTable[SINE_SIZE];
static int		sineReady;

static const float multipliers[16] = { 0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15 };
static const float kslPerOctave[4] = { 0, 3.0f, 1.5f, 6.0f };

/*
=====================================================================

THE OPERATORS

=====================================================================
*/

// the four OPL2 waveforms, phase in cycles
static float Wave( int waveform, float phase ) {
	float	s;
	int		half;

	phase -= floorf( phase );
	s = sineTable[(int)( phase * SINE_SIZE ) & ( SINE_SIZE - 1 )];
	half = phase >= 0.5f;
	switch ( waveform & 3 ) {
	case 1:		// half sine
		return half ? 0 : s;
	case 2:		// absolute sine
		return fabsf( s );
	case 3:		// quarter sine: the rising quarters of the absolute sine
		return ( phase - ( half ? 0.5f : 0 ) ) < 0.25f ? fabsf( s ) : 0;
	default:
		return s;
	}
}

// the time of a full envelope sweep for a rate (attack: 0 to full, decay
// and release: 0 to 96 dB), in seconds, from the YM3812 data sheet
static float RateTime( int rate, int rateShift, int attack ) {
	float	t;

	if ( rate <= 0 ) {
		return 0;		// never
	}
	t = ( attack ? 2.826f : 39.28f ) / (float)( 1 << ( rate - 1 ) );
	return t * powf( 2.0f, -rateShift / 4.0f );
}

static void KeyOn( opl_opstate_t *s ) {
	s->state = EG_ATTACK;
	s->attackAmp = 0;
	s->env = MAX_ATTENUATION;
	s->phase = 0;
	s->out1 = s->out2 = 0;
}

static void KeyOff( opl_opstate_t *s ) {
	if ( s->state != EG_OFF ) {
		s->state = EG_RELEASE;
	}
}

static void StepEnvelope( opl_opstate_t *s, const opl_op_t *op, int rateShift, float dt ) {
	int		ar = op->attack >> 4, dr = op->attack & 15, sl = op->sustain >> 4, rr = op->sustain & 15;
	float	sustainDb = sl == 15 ? 93.0f : sl * 3.0f, t;

	switch ( s->state ) {
	case EG_ATTACK:
		if ( ar == 15 ) {
			s->attackAmp = 1;
		} else {
			t = RateTime( ar, rateShift, 1 );
			if ( t > 0 ) {
				s->attackAmp += dt / t;
			}
		}
		if ( s->attackAmp >= 1 ) {
			s->attackAmp = 1;
			s->env = 0;
			s->state = EG_DECAY;
		} else {
			s->env = s->attackAmp > 0.00002f ? -20.0f * log10f( s->attackAmp ) : MAX_ATTENUATION;
		}
		break;
	case EG_DECAY:
		t = RateTime( dr, rateShift, 0 );
		if ( t > 0 ) {
			s->env += MAX_ATTENUATION * dt / t;
		}
		if ( s->env >= sustainDb ) {
			s->env = sustainDb;
			s->state = EG_SUSTAIN;
		}
		break;
	case EG_SUSTAIN:
		// a percussive envelope (EG type clear) goes on with the release rate
		if ( !( op->characteristic & 0x20 ) ) {
			t = RateTime( rr, rateShift, 0 );
			if ( t > 0 ) {
				s->env += MAX_ATTENUATION * dt / t;
			}
		}
		break;
	case EG_RELEASE:
		t = RateTime( rr, rateShift, 0 );
		if ( t > 0 ) {
			s->env += MAX_ATTENUATION * dt / t;
		}
		break;
	}
	if ( s->state != EG_ATTACK && s->env >= MAX_ATTENUATION ) {
		s->env = MAX_ATTENUATION;
		if ( s->state != EG_DECAY ) {
			s->state = EG_OFF;
		}
	}
}

// an operator's output for this sample: -1..1 scaled by its attenuation
static float Operator( opl_opstate_t *s, const opl_op_t *op, float freq, float phaseMod, float extraDb, float keyAtten,
	float tremolo, float vibrato, float dt ) {
	float	att, out;

	att = s->env + ( op->level & 0x3f ) * 0.75f + kslPerOctave[op->scale >> 6] * keyAtten + extraDb;
	if ( op->characteristic & 0x80 ) {
		att += tremolo;
	}
	out = att >= MAX_ATTENUATION ? 0 : Wave( op->waveform, s->phase + phaseMod ) * powf( 10.0f, -att / 20.0f );
	s->phase += freq * multipliers[op->characteristic & 15] * ( op->characteristic & 0x40 ? vibrato : 1.0f ) * dt;
	if ( s->phase > 1024.0f ) {
		s->phase -= floorf( s->phase );
	}
	return out;
}

/*
=====================================================================

VOICES

=====================================================================
*/

static void UpdateFrequency( doom_music_t *m, opl_voice_t *v ) {
	float	note = v->note;
	int		octave;

	if ( !( v->instrument->flags & 1 ) ) {
		note += m->channels[v->channel].bend;
	}
	// the second voice is detuned by the fine tuning, in 1/64 semitones
	if ( v->second ) {
		note += ( v->instrument->fineTuning / 2 - 64 ) / 32.0f;
	}
	v->freq = 440.0f * powf( 2.0f, ( note - 69.0f ) / 12.0f );

	// key scaling: the OPL block (octave) of the note
	octave = (int)( note / 12.0f ) - 1;
	if ( octave < 0 ) {
		octave = 0;
	} else if ( octave > 7 ) {
		octave = 7;
	}
	v->keyAtten = octave > 3 ? (float)( octave - 3 ) : 0;
	v->rateShift = octave;
}

// the channel's volume as an attenuation, dB
static float VoiceVolumeDb( doom_music_t *m, const opl_voice_t *v ) {
	const mus_channel_t	*ch = &m->channels[v->channel];
	float				vol = v->noteVolume * ch->volume * ch->expression;

	if ( vol <= 0.0001f ) {
		return MAX_ATTENUATION;
	}
	// DMX's volume curve is steeper than linear
	return -40.0f * log10f( vol );
}

static opl_voice_t *AllocVoice( doom_music_t *m ) {
	opl_voice_t	*best = NULL;
	int			i;

	for ( i = 0 ; i < MAX_VOICES ; i++ ) {
		if ( !m->voices[i].active ) {
			return &m->voices[i];
		}
	}
	// all busy: the oldest released voice, else the oldest
	for ( i = 0 ; i < MAX_VOICES ; i++ ) {
		opl_voice_t *v = &m->voices[i];

		if ( v->op[1].state == EG_RELEASE && ( !best || v->age < best->age ) ) {
			best = v;
		}
	}
	if ( !best ) {
		for ( i = 0 ; i < MAX_VOICES ; i++ ) {
			if ( !best || m->voices[i].age < best->age ) {
				best = &m->voices[i];
			}
		}
	}
	return best;
}

static void NoteOn( doom_music_t *m, int channel, int key, int volume ) {
	const opl_instrument_t	*ins;
	int						num, i, count;

	if ( channel == MUS_PERCUSSION ) {
		if ( key < 35 || key > 81 ) {
			return;
		}
		num = 128 + key - 35;
	} else {
		num = m->channels[channel].instrument;
	}
	if ( num < 0 || num >= GENMIDI_INSTRUMENTS ) {
		return;
	}
	ins = &m->instruments[num];
	count = ( ins->flags & 4 ) ? 2 : 1;

	for ( i = 0 ; i < count ; i++ ) {
		opl_voice_t	*v = AllocVoice( m );

		memset( v, 0, sizeof( *v ) );
		v->active = 1;
		v->channel = channel;
		v->key = key;
		v->age = m->age++;
		v->instrument = ins;
		v->def = &ins->voices[i];
		v->second = i;
		v->note = (float)( ( ins->flags & 1 ) ? ins->fixedNote : key ) + v->def->baseNote;
		v->noteVolume = volume / 127.0f;
		UpdateFrequency( m, v );
		KeyOn( &v->op[0] );
		KeyOn( &v->op[1] );
	}
}

static void NoteOff( doom_music_t *m, int channel, int key ) {
	int		i;

	for ( i = 0 ; i < MAX_VOICES ; i++ ) {
		opl_voice_t *v = &m->voices[i];

		if ( v->active && v->channel == channel && v->key == key && v->op[1].state != EG_RELEASE ) {
			KeyOff( &v->op[0] );
			KeyOff( &v->op[1] );
		}
	}
}

static void AllNotesOff( doom_music_t *m, int channel, int silence ) {
	int		i;

	for ( i = 0 ; i < MAX_VOICES ; i++ ) {
		opl_voice_t *v = &m->voices[i];

		if ( v->active && ( channel < 0 || v->channel == channel ) ) {
			if ( silence ) {
				v->active = 0;
			} else {
				KeyOff( &v->op[0] );
				KeyOff( &v->op[1] );
			}
		}
	}
}

static void ResetChannels( doom_music_t *m ) {
	int		i;

	for ( i = 0 ; i < MUS_CHANNELS ; i++ ) {
		mus_channel_t *ch = &m->channels[i];

		ch->instrument = 0;
		ch->volume = 100 / 127.0f;
		ch->expression = 1;
		ch->pan = 0.5f;
		ch->bend = 0;
		ch->noteVolume = 127;
	}
}

/*
=====================================================================

THE SCORE

=====================================================================
*/

static int ReadByte( doom_music_t *m ) {
	if ( m->pos >= m->scoreLen ) {
		return -1;
	}
	return m->score[m->pos++];
}

static void Controller( doom_music_t *m, int channel, int ctrl, int value ) {
	mus_channel_t	*ch = &m->channels[channel];

	if ( value > 127 ) {
		value = 127;
	}
	switch ( ctrl ) {
	case 0:		// instrument
		ch->instrument = value;
		break;
	case 3:		// volume
		ch->volume = value / 127.0f;
		break;
	case 4:		// pan
		ch->pan = value / 127.0f;
		break;
	case 5:		// expression
		ch->expression = value / 127.0f;
		break;
	}
}

static void SystemEvent( doom_music_t *m, int channel, int ctrl ) {
	switch ( ctrl ) {
	case 10:	// all sounds off
		AllNotesOff( m, channel, 1 );
		break;
	case 11:	// all notes off
		AllNotesOff( m, channel, 0 );
		break;
	case 14:	// reset all controllers
		m->channels[channel].volume = 100 / 127.0f;
		m->channels[channel].expression = 1;
		m->channels[channel].pan = 0.5f;
		m->channels[channel].bend = 0;
		break;
	}
}

// the events of one tick; the delay to the next, in ticks, or -1 at the end
static int PlayEvents( doom_music_t *m ) {
	for ( ;; ) {
		int		event = ReadByte( m ), channel, type, a, i, delay;

		if ( event < 0 ) {
			return -1;
		}
		channel = event & 15;
		type = ( event >> 4 ) & 7;

		switch ( type ) {
		case 0:		// release note
			a = ReadByte( m );
			NoteOff( m, channel, a & 127 );
			break;
		case 1:		// play note, with a volume if bit 7 is set
			a = ReadByte( m );
			if ( a & 128 ) {
				m->channels[channel].noteVolume = ReadByte( m ) & 127;
			}
			NoteOn( m, channel, a & 127, m->channels[channel].noteVolume );
			break;
		case 2:		// pitch bend: 128 is none, 64 a semitone down
			a = ReadByte( m );
			m->channels[channel].bend = ( a - 128 ) / 64.0f;
			for ( i = 0 ; i < MAX_VOICES ; i++ ) {
				if ( m->voices[i].active && m->voices[i].channel == channel ) {
					UpdateFrequency( m, &m->voices[i] );
				}
			}
			break;
		case 3:		// system event
			SystemEvent( m, channel, ReadByte( m ) & 127 );
			break;
		case 4:		// controller
			a = ReadByte( m );
			Controller( m, channel, a & 127, ReadByte( m ) & 127 );
			break;
		case 5:		// end of measure
			break;
		case 6:		// score end
			return -1;
		default:	// unused, one data byte
			ReadByte( m );
			break;
		}

		if ( event & 128 ) {
			delay = 0;
			do {
				a = ReadByte( m );
				if ( a < 0 ) {
					return -1;
				}
				delay = delay * 128 + ( a & 127 );
			} while ( a & 128 );
			if ( delay > 0 ) {
				return delay;
			}
		}
	}
}

/*
=====================================================================

THE PLAYER

=====================================================================
*/

static void ParseVoice( const uint8_t *p, opl_voicedef_t *v ) {
	v->mod.characteristic = p[0];
	v->mod.attack = p[1];
	v->mod.sustain = p[2];
	v->mod.waveform = p[3];
	v->mod.scale = p[4];
	v->mod.level = p[5];
	v->feedback = p[6];
	v->car.characteristic = p[7];
	v->car.attack = p[8];
	v->car.sustain = p[9];
	v->car.waveform = p[10];
	v->car.scale = p[11];
	v->car.level = p[12];
	v->baseNote = (int16_t)doom_u16( p + 14 );
}

doom_music_t *doom_music_open( const doom_wad_t *wad, const char *name, int rate, int loop ) {
	int				musLump = doom_wad_find( wad, name ), genLump = doom_wad_find( wad, "GENMIDI" ), i;
	const uint8_t	*mus, *gen;
	uint32_t		musSize;
	int				start, len;
	doom_music_t	*m;

	if ( musLump < 0 || genLump < 0 || rate <= 0 ) {
		return NULL;
	}
	mus = doom_wad_lump_data( wad, musLump );
	musSize = doom_wad_lump( wad, musLump )->size;
	gen = doom_wad_lump_data( wad, genLump );
	if ( musSize < 16 || memcmp( mus, "MUS\x1a", 4 ) || doom_wad_lump( wad, genLump )->size < GENMIDI_HEADER + GENMIDI_INSTRUMENTS * GENMIDI_SIZE
		|| memcmp( gen, "#OPL_II#", 8 ) ) {
		return NULL;
	}
	len = doom_u16( mus + 4 );
	start = doom_u16( mus + 6 );
	if ( start >= (int)musSize ) {
		return NULL;
	}
	if ( start + len > (int)musSize ) {
		len = (int)musSize - start;
	}

	if ( !sineReady ) {
		for ( i = 0 ; i < SINE_SIZE ; i++ ) {
			sineTable[i] = sinf( 2 * PI_F * i / SINE_SIZE );
		}
		sineReady = 1;
	}

	m = calloc( 1, sizeof( *m ) );
	if ( !m ) {
		return NULL;
	}
	m->rate = rate;
	m->loop = loop;
	m->score = mus + start;
	m->scoreLen = len;
	for ( i = 0 ; i < GENMIDI_INSTRUMENTS ; i++ ) {
		const uint8_t		*p = gen + GENMIDI_HEADER + i * GENMIDI_SIZE;
		opl_instrument_t	*ins = &m->instruments[i];

		ins->flags = doom_u16( p );
		ins->fineTuning = p[2];
		ins->fixedNote = p[3];
		ParseVoice( p + 4, &ins->voices[0] );
		ParseVoice( p + 20, &ins->voices[1] );
	}
	ResetChannels( m );
	return m;
}

void doom_music_close( doom_music_t *m ) {
	free( m );
}

/*
=================
doom_music_render

Up to 'frames' stereo 16-bit frames; fewer once a non-looping score ends
=================
*/
int doom_music_render( doom_music_t *m, int16_t *out, int frames ) {
	float	dt = 1.0f / m->rate, samplesPerTick = (float)m->rate / MUS_TICRATE;
	int		f, i;

	for ( f = 0 ; f < frames ; f++ ) {
		float	left = 0, right = 0, tremolo, vibrato;
		int		active = 0;

		// the score
		while ( !m->done && m->samplesToEvent <= 0 ) {
			int delay = PlayEvents( m );

			if ( delay < 0 ) {
				if ( m->loop ) {
					m->pos = 0;
					AllNotesOff( m, -1, 0 );
					ResetChannels( m );
					delay = 1;
				} else {
					m->done = 1;
					AllNotesOff( m, -1, 0 );
					break;
				}
			}
			m->samplesToEvent += delay * samplesPerTick;
		}
		m->samplesToEvent -= 1;

		// the chip's low frequency oscillators: 1 dB tremolo at 3.7 Hz,
		// 7 cents vibrato at 6.1 Hz
		m->lfoPhase += dt;
		if ( m->lfoPhase > 1000.0f ) {
			m->lfoPhase -= 1000.0f;
		}
		tremolo = 0.5f * ( 1 + sinf( 2 * PI_F * 3.7f * m->lfoPhase ) );
		vibrato = powf( 2.0f, 7.0f * sinf( 2 * PI_F * 6.1f * m->lfoPhase ) / 1200.0f );

		for ( i = 0 ; i < MAX_VOICES ; i++ ) {
			opl_voice_t				*v = &m->voices[i];
			const opl_voicedef_t	*d;
			float					volDb, fb, mod, car, s, pan;
			int						additive;

			if ( !v->active ) {
				continue;
			}
			active++;
			d = v->def;
			additive = d->feedback & 1;
			volDb = VoiceVolumeDb( m, v );

			StepEnvelope( &v->op[0], &d->mod, ( d->mod.characteristic & 0x10 ) ? v->rateShift * 2 : v->rateShift / 2, dt );
			StepEnvelope( &v->op[1], &d->car, ( d->car.characteristic & 0x10 ) ? v->rateShift * 2 : v->rateShift / 2, dt );

			// the modulator with its self-feedback
			fb = ( d->feedback >> 1 ) & 7;
			fb = fb ? ( v->op[0].out1 + v->op[0].out2 ) * powf( 2.0f, fb - 6 ) * 0.5f : 0;
			mod = Operator( &v->op[0], &d->mod, v->freq, fb, additive ? volDb : 0, v->keyAtten, tremolo, vibrato, dt );
			v->op[0].out2 = v->op[0].out1;
			v->op[0].out1 = mod;

			// the carrier, phase-modulated by the modulator or added to it
			car = Operator( &v->op[1], &d->car, v->freq, additive ? 0 : mod * 2.0f, volDb, v->keyAtten, tremolo, vibrato, dt );
			s = additive ? car + mod : car;

			pan = m->channels[v->channel].pan;
			left += s * ( 1 - pan ) * 1.414f;
			right += s * pan * 1.414f;

			if ( v->op[1].state == EG_OFF && ( !additive || v->op[0].state == EG_OFF ) ) {
				v->active = 0;
			}
		}

		if ( m->done && !active ) {
			return f;
		}
		left *= 0.25f * 32767;
		right *= 0.25f * 32767;
		out[f * 2] = (int16_t)( left > 32767 ? 32767 : left < -32768 ? -32768 : left );
		out[f * 2 + 1] = (int16_t)( right > 32767 ? 32767 : right < -32768 ? -32768 : right );
	}
	return frames;
}
