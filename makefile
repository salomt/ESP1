esp1: esp1.c notelist.c
	cc -o ESP1 esp1.c notelist.c -lportaudio -lportmidi -framework CoreAudio
