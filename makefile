esp1: esp1.c notelist.c
	cc -o esp1 esp1.c notelist.c -lportaudio -lportmidi -framework CoreAudio
