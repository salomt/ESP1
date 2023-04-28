
#include "notelist.h"


/* -------------------------------------------------------------------------------------
	createNotelist 
------------------------------------------------------------------------------------- */
MIDInote *createNotelist()
{
	MIDInote *list = malloc(sizeof(MIDInote));
	list->next = NULL;
	list->note = -1;
	list->vel = -1;
	list->chan = -1;
	return list;
}


/* -----------------------------------------------------------------------------------
	resetNotelist : deletes all notes from the list, except the first, dummy note.
------------------------------------------------------------------------------------- */
int resetNotelist(MIDInote *list)
{
	MIDInote *tmp = list->next, *tmp2;
	
	while (tmp != NULL) {
			tmp2 = tmp->next;				    /*  printf("freed %d\n", tmp->note); */
			free(tmp);	
			tmp = tmp2;
	}
}

/* -----------------------------------------------------------------------------------
	addNote: 
------------------------------------------------------------------------------------- */
void addNote(MIDInote *list, unsigned char chan, unsigned char note, unsigned char vel)
{
	MIDInote *tmp;
	tmp = list;
	while (tmp->next != NULL) {
		tmp = tmp->next; 
	}
	tmp->next = malloc(sizeof(MIDInote));                                  	  /*printf("created new note\n"); */
	tmp = tmp->next;
	tmp->note = note;
	tmp->vel = vel;
	tmp->next = NULL;
}

/* ---------------------------------------------------------------------------------------
	removeNote:
-------------------------------------------------------------------------------------------- */
void removeNote(MIDInote *list, unsigned char chan, unsigned char note)
{
	MIDInote *tmp, *tmp2;
	tmp = list;
	while (tmp->next != NULL) {
		if (tmp->next->note == note) {
			/* remove note from the middle of the list */
			if (tmp->next->next != NULL) {
				tmp2 = tmp->next->next;
				free(tmp->next);												/* printf("freed note from middle\n");  */
				tmp->next = tmp2;
			}
			/* remove note from the end */
			else {
				free(tmp->next);												/* printf("freed last note\n"); */
				tmp->next = NULL;
				break; /* when last note is removed, get out of the while loop */
			}
		}	
		tmp = tmp->next;
	}
}

/* -------------------------------------------------------------------------------------------
	lastNote 
--------------------------------------------------------------------------------------------- */
short lastNote(MIDInote *list)
{
	if (list == NULL)
		return -1;
	MIDInote *tmp = list;
	while (tmp->next != NULL) {
		tmp = tmp->next;		
	} 
	return tmp->note;
}

