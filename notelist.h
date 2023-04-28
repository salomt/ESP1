
/*-----------------------------------------------------------------------------------  
    NOTELIST
    Copyright (c) 2008 Tommi Salomaa

    Data structure for storing midi note events. Does not store time information.

	-storing note-on message for a note which is already on does not work well

	last modification: 3.5.2008

----------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

/* Note events are stored into a linked list with note number and channel data. */
typedef struct MIDInote {
	short note;
	short vel;
	short chan;
	struct MIDInote *next;
} MIDInote;


/*---------------------------------------------------------------------------
	createNotelist returns a pointer to first 'dummy' node of a notelist
------------------------------------------------------------------------------*/
MIDInote *createNotelist();

/*--------------------------------------------------------------------------
 	resetNotelist deletes all notes in the notelist 
----------------------------------------------------------------------------*/
int resetNotelist(MIDInote *list);


/* -----------------------------------------------------------------------------
	addNote adds a note to the list, by midi channel and velocity 
------------------------------------------------------------------------------*/
void addNote(MIDInote *list, unsigned char chan, unsigned char note, unsigned char vel); 

/* ----------------------------------------------------------------------------
	removes given note on given channel from the list
-------------------------------------------------------------------------------*/
void removeNote(MIDInote *list, unsigned char chan, unsigned char note); 

/*----------------------------------------------------------------------------
	lastNote returns the last note in the notelist. In monophonic context the 
	last received note is usually the note that plays.
	If there are no notes lastNote returns value of -1.             
-----------------------------------------------------------------------------*/
short lastNote(MIDInote *list);

