/* includes */

#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/* defines */

#define CTRL_KEY(k) ((k) & 0x1f)

/* data */

struct editorConfig {
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

//prints out an error message for the failing function
void die(const char *s){
	// \x1b is an escape character
	write(STDOUT_FILENO, "\x1b[2J", 4);//clear
	write(STDOUT_FILENO, "\x1b[H", 3);//move cursor to 1,1

	perror(s);
	exit(1);
}

/* terminal */

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);

	struct termios raw = E.orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);//disable ctrl S and ctrl Q
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);//disable ECHO, canonical mode, ctrl v, c and z
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VMIN] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey(){
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* output */

void editorDrawsRows(void){
	int y;
	for (y=0; y<E.screenrows; y++) {
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void editorRefreshScreen(void){
	// \x1b is an escape character
	write(STDOUT_FILENO, "\x1b[2J", 4);//clear
	write(STDOUT_FILENO, "\x1b[H", 3);//move cursor to 1,1

	editorDrawsRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/* input */

void editorProcessKeypress(void){
	char c = editorReadKey();

	switch (c) {
		case CTRL_KEY('q'):
			exit(0);
			break;
	}

}

/* init */

void initEditor(void){	
	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(){
	enableRawMode();
	initEditor();

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}
