######################################################################

all: delay_stereo.so

delay_stereo.so: delay_stereo.o
	gcc -o delay_stereo.so delay_stereo.o -shared -Wall -fPIC -Werror -O2 -fvisibility=hidden -fvisibility-inlines-hidden -s 

delay_stereo.o: delay_stereo.c
	gcc -o delay_stereo.o -c delay_stereo.c

clean:
	rm delay_stereo.o delay_stereo.so

######################################################################
