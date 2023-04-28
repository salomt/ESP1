/*-----------------------------------------------------------------------------------------
 
	ESP-1  Experiment in Synthesizer Programming - 1
                 
  	Copyright (c) 2008 Tommi Salomaa tommi.salomaa@helsinki.fi
  
  	This program uses PortAudio and PortMidi libraries.
  	For more information see: http://www.cs.cmu.edu/~music/portmusic/
  	PortMusic: Copyright (c) 1999-2000 Ross Bencina and Phil Burk, (c) 2001-2006 Roger B. Dannenberg
  
	Permission is hereby granted, free of charge, to any person obtaining
  	a copy of this software and associated documentation files
  	(the "Software"), to deal in the Software without restriction,
  	including without limitation the rights to use, copy, modify, merge,
  	publish, distribute, sublicense, and/or sell copies of the Software,
 	and to permit persons to whom the Software is furnished to do so,
 	subject to the following conditions:
 
  	The above copyright notice and this permission notice shall be
 	included in all copies or substantial portions of the Software.
 
 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 	OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 
  	This program is a simple midi-controlled tone generator. The program can produce 
 	four different waveforms: pulse, triangle, sawtooth and sine wave. Furthermore,
	the pulsewidth of the pulse wave can be set. There is an adjustable envelope for
  	controlling amplitude, and a slight vibrato.  
  
  	The main purpose of this software is to experiment with programming techniques needed
  	to generate musical tones and control them with midi data. 
  
  	Last modified: 21.5.2008

-------------------------------------------------------------------------------------------*/
	
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <portaudio.h>
#include <portmidi.h>
#include <porttime.h>
#include <pmutil.h>
#include "notelist.h"

/* the 12th square root of two for note frequency calculations */
#define SQU_12 1.0594630943

/* the frequency of midi note number 0 */
#define NOTE_0_FREQ 8.1757989156

/* waveforms */
#define PUL 1
#define TRI 2
#define SAW 3
#define SIN 4

/* envelope stages */
#define ATT 0
#define DEC 1
#define SUS 2
#define REL 3
#define OFF 4

#define ATT_MAX 3000
#define DEC_MAX 3000
#define SUS_MAX 100
#define REL_MAX 3000

/* midi message types */
#define NOTE_ON  0x90 
#define NOTE_OFF 0x80 
#define CONTROL  0xB0
#define CH_PRESS 0xD0
#define PITCH_WH 0xE0

/* midi controller numbers */
#define MOD_WHEEL  1
#define BREATH     2
#define FOOT_PEDAL 3
#define DATA_ENTRY 6
#define MIDI_VOL   7
#define SLIDER_1   16
#define SLIDER_2   17
#define SLIDER_3   18
#define SLIDER_4   19
#define HOLD_PEDAL 64

#define PWHEEL_MID 64
#define PWHEEL_RANGE 2
       
/* controller destinations */
#define VOLUME        1
#define WAVEFORM      2
#define FREQUENCY     3
#define PULSEWIDTH    4
#define VIBRATO_DEPTH 5
#define VIBRATO_RATE  6
#define ENV_ATTACK    7
#define ENV_DECAY     8
#define ENV_SUSTAIN   9
#define ENV_RELEASE   10
#define HOLD          11


/* envelope */
typedef struct
{
	float level;
	float timebase; 	
	int   stage;
	unsigned int value[4]; /* ATT, DEC, SUS, REL values (see #define above) for SUS the value is % of max amp. OFF needs no time */
	unsigned int max_val[4];
} envelope;


/* mididata structure */
typedef struct
{
	PmQueue *event_queue; /* queue for transferring midi event between threads */
	MIDInote *notelist;   /* linked list for notes */
	int keysDown;         /* tells how many keys are pressed down. Needed for example the implementation of hold pedal. */
	int pwheel;           /* pitch wheel state has to be stored, because the state must be retained after other events. */
	int chpress;          /* channel pressure */
	int hold;             /* the state of hold pedal */
} MidiData;


/* audio data */
typedef struct
{
    float  left;      /* left output data      */
    float  right;     /* right output data     */
    float  phase;     /* phase                 */
    int    waveform;  /* the type of waveform  */
	float  gain;	
    float  freq;      /* current frequency     */
	float  mfreq;     /* modulation of freq    */
    float  pw;        /* pulsewidth            */
    float  amp;       /* the amplitude factor  */
    float  max;       /* maximum amplitude     */
    float  fp;        /* freq modulator phase  */
    float  ofreq;     /* original freq         */ 
    float  vdepth;    /* vibrato depth         */
	float  vrate;     /* vibrato rate          */

	envelope   *env;  /* envelope controlling volume */

	int ctdest[128];  /* indicates destinations for midi controllers */

} AudioData;


/* synthdata, a combination of audio and midi data */
typedef struct
{
	AudioData *ad;
	MidiData *md;	
} SynthData;


/* globals ---------------------------------------------------------------------------- */
PaStream *stream;  /* audio stream */
PmStream *midi_in; /* midi input stream */

int midi_in_open;  
unsigned int samplerate; 
unsigned int framecount;

/*------------------------------------------------------------------------------------
	note_to_freq:
	convert midi note number to frequency                      
--------------------------------------------------------------------------------------*/
float note_to_freq(int notenum)
{ 
	return NOTE_0_FREQ * (pow(SQU_12, notenum));
}

/* ------------------------------------------------------------------------------------------
	readInt: reads an integer between min and max from console
--------------------------------------------------------------------------------------------*/
int readInt(int min, int max)
{
	char line[100];
	int ready = 0;
	int result;

	while (!ready) {
		fgets(line, 100, stdin);
		result = atoi(line);
		/* check that input is within boundaries, and that the first char is a digit */
		if (result >= min && result <= max && (int)line[0] >= 48 && (int)line[0] <= 57)
			ready = 1;
		else
			printf("Give a number between %d-%d!\n", min, max);
	}
	return result;
}


/* -------------------------------------------------------------------------------------
	handleMidiEvent: midi event is interpreted and changes applied to
	the audio data
---------------------------------------------------------------------------------------- */
void handleMidiEvent(PmEvent *ev, SynthData *syn)
{
	unsigned char status = Pm_MessageStatus(ev->message), /* status byte */
				 	data1 = Pm_MessageData1(ev->message), /* first data byte */
					data2 = Pm_MessageData2(ev->message); /* second data byte */

	/* get the message type and channel from the status byte by bit-masking */
	unsigned char msg = status & 0xF0;
	unsigned char chan = status & 0x0F;
	
	/* Some devices use note_on with velocity 0 to indicate note_off. If an event like this is
		detected, the message is changed to note_off. */
	if (msg == NOTE_ON && data2 == 0) msg = NOTE_OFF;

	switch (msg) {
		/* NOTE_ON: note is added to the notelist, and envelope is re-triggered if it is off or in rel stage, or it is in sustain phase
		   and the sustain value is 0 
		   the velocity of the note is calculated as well */
		case NOTE_ON:
			syn->md->keysDown++;
			addNote(syn->md->notelist, chan, data1, data2);
			if (syn->ad->env->stage == OFF || syn->ad->env->stage == REL ||
			(syn->ad->env->stage == SUS && syn->ad->env->value[SUS] == 0)) { 
				syn->ad->env->stage = ATT;
				syn->ad->max = 0.2 + data2 * 0.00629921; /* FIXME: Here should be a better calculation */
			}
			break;

		/* NOTE_OFF: note is removed from the notelist */
		case NOTE_OFF:
			syn->md->keysDown--;
			removeNote(syn->md->notelist, chan, data1);
			if (lastNote(syn->md->notelist) < 0 && !syn->md->hold) /* If the notelist is empty and hold pedal is not pressed, the  */
				syn->ad->env->stage = REL;                    /* envelope is put to release stage.                            */ 
			break;

		case PITCH_WH:   /* pitch wheel -the calculation does not work if PWHEEL_RANGE > 2 */
			syn->md->pwheel = data2; // TODO: the combining of the two data byte values for a 14-bit value (see midi spec.)
			syn->ad->freq = syn->ad->ofreq + (syn->md->pwheel - PWHEEL_MID) * ((pow(SQU_12, PWHEEL_RANGE) / 64) * 0.1) * syn->ad->ofreq;
			break;

		case CH_PRESS: 
			if (data1 > 0)                      /* channel pressure: vibrato depth */
				syn->ad->vdepth = data1 * 0.05;       
			else
				syn->ad->vdepth = 0.5;
			break;

		case CONTROL: /* controllers are handled according to the ctdest array */
			switch (syn->ad->ctdest[data1]) {
				case VOLUME:
					syn->ad->gain = 0.00787 * data2;	
					break;	
				case WAVEFORM:
					syn->ad->waveform = 0.031 * data2 + 1;	
					break;
				case PULSEWIDTH:	            
					syn->ad->pw = 5 + (data2 * 0.354);
					break;
				case VIBRATO_DEPTH:		 
					break;
				case VIBRATO_RATE:
					break;
				case ENV_ATTACK:
					syn->ad->env->value[ATT] = data2 * (syn->ad->env->max_val[ATT] / 127);
					break;
				case ENV_DECAY:
					syn->ad->env->value[DEC] = data2 * (syn->ad->env->max_val[DEC] / 127);
					break;
				case ENV_SUSTAIN:
					syn->ad->env->value[SUS] = data2 * 0.7874; /* why only this works? */
					break;
				case ENV_RELEASE:
					syn->ad->env->value[REL] = data2 * (syn->ad->env->max_val[REL] / 127);
					break;
				case HOLD:
					syn->md->hold = !(syn->md->hold);
					if (!syn->md->hold && syn->md->keysDown == 0) 
						syn->ad->env->stage = REL; 
					break;
				default:
					break;
			}
	}

	/* in case of note on/off, determine which note will be played, or put envelope in release if there are no notes on the list */
	/* the frequency of the note is determined here */
	if (msg == NOTE_ON || msg == NOTE_OFF) {
		if (lastNote(syn->md->notelist) >= 0) {
			syn->ad->freq = note_to_freq(lastNote(syn->md->notelist));
			syn->ad->ofreq = syn->ad->freq;
			syn->ad->freq = syn->ad->ofreq + (syn->md->pwheel - PWHEEL_MID) * ((pow(SQU_12, PWHEEL_RANGE) / 64) * 0.1) * syn->ad->ofreq;
			 /* pitch wheel calculation FIXME: what is the proper place for frequency calculation, including pitch wheel?
				should it be made in the audio callback function?  */
		}
	}
}


/* --------------------------------------------------------------------------------------------------
	poll_midi: Callback function for porttime. Midi events are read from selected input 
	port and sent to pa_callback function via the event_queue.
--------------------------------------------------------------------------------------------------- */
void poll_midi(PtTimestamp timestamp, void *userData)
{
	SynthData *data = (SynthData*)userData; 
	PmEvent event;
	if (midi_in_open) { 
		if (Pm_Read(midi_in, &event, 1) == 1 && !Pm_QueueFull(data->md->event_queue)) {	/* read a midi event */
			Pm_Enqueue(data->md->event_queue, &event);	/* if an event is read, it is sent to audio callback function */
		} 
	}
}


/* ---------------------------------------------------------------------------------------------------
	pa_callback: Callback function for the audio stream. The audio data is calculated and written 
	into audio output. Possible midi event is also handled here.
------------------------------------------------------------------------------------------------------ */
static int pa_callback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				       void *userData)
{
    /* Cast data passed through stream to our structure. */
    SynthData *data = (SynthData*)userData; 
    float *out = (float*)outputBuffer;
    (void) inputBuffer; /* Prevent unused variable warning. */

	/* check if we have a midi event and handle it */  
	PmEvent event;
	if (midi_in_open && Pm_Dequeue(data->md->event_queue, &event) == 1) {
		handleMidiEvent(&event, data);
	}    
	/*
	   PmEvent event;
	if (midi_in_open && Pm_Dequeue(data->md->event_queue, &event) == 1)
			Only this works in linux???!!!
	*/

	unsigned int i;
	for(i = 0; i < framesPerBuffer; i ++ )
	{
		/* write audio data to output */ 
		*out++ = data->ad->gain * (data->ad->left * data->ad->amp);  
        *out++ = data->ad->gain * (data->ad->right * data->ad->amp); 

		/* ADSR envelope code -------------------------------------------------------------------------------------- */
		/* TODO: separation of env into own source file and made more generic */
		switch (data->ad->env->stage) {
			/* in ATT phase, the volume is increased until it is at the max value*/
			case ATT:
				if (data->ad->env->value[ATT] > 0) { 
					/* if the value of the stage is 0, the envelope goes to next stage */
					if (data->ad->amp < data->ad->max) 
						data->ad->amp += (data->ad->max / (data->ad->env->value[ATT] * data->ad->env->timebase)); 
						/* att, dec and rel times depend on the max amp value AND the samplerate. */
					else 
						data->ad->env->stage = DEC;
				}
				else 
					/* to prevent an audible pop, set att to 1 if it is 0, instead of jumping straight to max value */
					data->ad->env->value[ATT] = 1; 
				break;

			/* DEC: volume is decreased until it is at sustain value % of max, when ready, go to sus stage */
			case DEC:
				if (data->ad->env->value[DEC] > 0) {
					if (data->ad->amp > (data->ad->max * (data->ad->env->value[SUS] * 0.01)))
						data->ad->amp -= (data->ad->max / (data->ad->env->value[DEC] * data->ad->env->timebase));
					else {
						data->ad->env->stage = SUS;
					}
				}
 				/* if dec is 0, set amp value to sus % directly, and go to sus stage */
				else {
					data->ad->amp = data->ad->max * (data->ad->env->value[SUS] * 0.01);
					data->ad->env->stage = SUS;
				}			
				break;

			/* REL is triggered by a note off event,Volume is decreased to zero. if amp is 0, envelope is set to off */
			case REL:				
				if (data->ad->amp >= 0 && data->ad->env->value[REL] > 0)
					data->ad->amp -= (data->ad->max / (data->ad->env->value[REL] * data->ad->env->timebase)); 
				else {
					data->ad->amp = 0;
					data->ad->env->stage = OFF;
				}	
				break;
  		}

		/* calculations for different waveforms ------------------------------------------ */ 
		switch (data->ad->waveform) {
			case PUL: 
				if (data->ad->phase < (2 * M_PI / 100 * data->ad->pw)) 
					data->ad->left = 1.0; 	    
				else 
					data->ad->left = -1.0; 
				break;	
			case TRI: 
				if (data->ad->phase < M_PI)
					data->ad->left = -1 + (2 / M_PI) * data->ad->phase;
				else
					data->ad->left = 3 - (2 / M_PI) * data->ad->phase;
				break;		   
			case SAW: 
				data->ad->left = (1 - (1 / M_PI) * data->ad->phase);
				break; 
			case SIN: 
				data->ad->left = (sin(data->ad->phase));
        }

		/* waveform phase update ------------------------------------------------------*/
		data->ad->right = data->ad->left;
        data->ad->phase += ((2 * M_PI * data->ad->freq) / samplerate);
		if (data->ad->phase > (2 * M_PI))
			data->ad->phase -= (2 * M_PI);

		/* vibrato --------------------------------------------------------- */
		/* FIXME: the vibrato implementation is bad: for example, if vrate is changed suddenly from high value to to low,
			freq can be stuck with a wrong value */
		data->ad->freq += sin(data->ad->fp) * (0.000001 * (data->ad->vdepth * data->ad->ofreq));
        data->ad->fp += ((2 * M_PI * data->ad->vrate) / samplerate);
		if (data->ad->fp > (2 * M_PI)) {
			data ->ad->fp -= (2 * M_PI);
		}
	}
    return 0;
}


/* -----------------------------------------------------------------------
	initSynthData: reservation of memory and initialization of data
---------------------------------------------------------------------------*/
SynthData* initSynthData()
{
    SynthData *syn = malloc(sizeof(SynthData));
	
	syn->ad = malloc(sizeof(AudioData));
	syn->ad->waveform = 1;
    syn->ad->left = syn->ad->right = 0.0; 
	syn->ad->gain = 0.5;
	syn->ad->freq = 0; 
	syn->ad->ofreq = 0;
    syn->ad->pw = 50; 
	syn->ad->fp = 0; 
    syn->ad->amp = 0; 
	syn->ad->max = 0;
    syn->ad->vdepth = 0.5;
	syn->ad->vrate = 5;

    syn->ad->env = malloc(sizeof(envelope));
    syn->ad->env->stage = OFF;
    syn->ad->env->value[ATT] = 3; 
    syn->ad->env->value[DEC] = 180;
    syn->ad->env->value[SUS] = 60;
    syn->ad->env->value[REL] = 800;
	syn->ad->env->max_val[ATT] = ATT_MAX;
	syn->ad->env->max_val[DEC] = DEC_MAX;
	syn->ad->env->max_val[SUS] = SUS_MAX;
	syn->ad->env->max_val[REL] = REL_MAX;
	syn->ad->env->timebase = samplerate / 1000;

	/* assign controllers to default destinations */
	syn->ad->ctdest[MIDI_VOL] = VOLUME;
	syn->ad->ctdest[DATA_ENTRY] = WAVEFORM;
	syn->ad->ctdest[MOD_WHEEL] = PULSEWIDTH;
	syn->ad->ctdest[HOLD_PEDAL] = HOLD;
	
	/* NOTE: Kurzweil k2600 uses controller nums 22-28 for 
		its sliders. May not be used by other manufacturers */
	syn->ad->ctdest[22] = ENV_ATTACK;
	syn->ad->ctdest[23] = ENV_DECAY;
	syn->ad->ctdest[24] = ENV_SUSTAIN;
	syn->ad->ctdest[25] = ENV_RELEASE;

	syn->md = malloc(sizeof(MidiData));
	syn->md->keysDown = 0;
	syn->md->pwheel = PWHEEL_MID;
	syn->md->chpress = 0;
	syn->md->hold = 0;
	syn->md->notelist = createNotelist();
	syn->md->event_queue = Pm_QueueCreate(128, sizeof(PmEvent));
	
	return syn;
}

/* ------------------------------------------------------------------------
	closeData
----------------------------------------------------------------------------*/
void closeData(SynthData *syn)
{
	if (midi_in_open) {
		Pt_Stop();
		midi_in_open = 0;
		Pm_Close(midi_in);
		Pm_Terminate();
	}

	Pa_Sleep(1000);
	
	resetNotelist(syn->md->notelist);
	free(syn->md->notelist); 
	Pm_QueueDestroy(syn->md->event_queue);  
	free(syn->md);
	free(syn->ad->env);
	free(syn->ad);
	free(syn);	
}

/* ------------------------------------------------------------------------------------
	openAudioStream
-------------------------------------------------------------------------------------- */
PaError openAudioStream(SynthData *syn)
{
	PaError err;
    err = Pa_Initialize();
    if(err != paNoError) return err;
	
	err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, samplerate, framecount, pa_callback, syn);
	if (err != paNoError) return err;

	err = Pa_StartStream(stream);
	if(err != paNoError) return err;
	
	return paNoError;
}

/* ---------------------------------------------------------------------------------------
	openMidiPort
----------------------------------------------------------------------------------------- */
int openMidiPort(SynthData *syn)
{
    Pm_Initialize(); 
	Pt_Start(1, poll_midi, syn);
	
	int deviceNum;
	int midiDev[10]; /* array for numbering inputs so that they start from 1 */
    int i, j = 0;    
		  
	printf("Choose midi input device:\n");
	for (i=0; i < Pm_CountDevices(); i++) {  
		const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
		if (info->input) {
			j++;
			midiDev[j] = i;       
			printf(" %d: %s, %s\n", j, info->interf, info->name);
		}	
	}
	if (j != 0)
		deviceNum = readInt(1, j);
	else {
		printf("No midi in ports found.\n");
		return 1;
	}
   if (Pm_OpenInput(&midi_in, midiDev[deviceNum], NULL, 512, NULL, 0) == 0) {
		midi_in_open = 1; 
	}
	else {
		printf("Cannot open midi port.\n");
		return 1;
	} 
	return 0;
}

/*------------------------------------------------------------------------------------------- 
  main
---------------------------------------------------------------------------------------------*/
int main(void)
{		
	PaError err;   
    int done = 0; 				 	
	midi_in_open = 0;

	samplerate = 44100;
	framecount = 128; /* how many frames are written at once in the audio callback -affects midi latency */

    SynthData *synth = initSynthData();

	err = openAudioStream(synth);
	if (err != paNoError) goto error;

	done = openMidiPort(synth);

    /* main loop ------------------------------------------------------------------------------------- */
	while (!done) {

		printf("Choose action:\n");
		printf(" 1: set waveform\n 2: set envelope\n 0: quit\n");
			
		int sel = readInt(0, 2);
		
		switch (sel) {
			case 1:
				printf(" 1: pulse\n 2: triangle\n 3: sawtooth\n 4: sine\n");
				synth->ad->waveform = readInt(0, 4);
				break;
			case 2:
				printf("Set attack, decay, sustain and release values:\n");
				int i;
				for (i = 0; i < 4; i++) {
					synth->ad->env->value[i] = readInt(0, synth->ad->env->max_val[i]);
				} 
				break;
			case 0:
				done = 1;
		}		
	}
    
	  
	/* closeup ----------------------------------------------------------------------------------------- */
	if (Pa_IsStreamActive(stream)) {
		err = Pa_StopStream(stream);
		if(err != paNoError) goto error;
    
		err = Pa_CloseStream(stream);
		if(err != paNoError) goto error;
	}
	
	Pa_Terminate();
	closeData(synth);
 
 	printf("Finished.\n");
	return err;
    
error:
   	Pa_Terminate();
	closeData(synth);
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
