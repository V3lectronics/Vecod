/* includes */

#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

/* defines */

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

/* data */

struct editorConfig {
	int cx, cy;
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

int getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;	
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;	
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;	

	return 0;
}

/* append buffer */

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

//append to buffer
void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

//free buffer memory
void abFree(struct abuf *ab){
	free(ab->b);
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;

	//wtf
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[99C\x1b[99B", 12) != 12) return	-1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* output */

void editorDrawsRows(struct abuf *ab){
	int y;
	for (y=0; y<E.screenrows; y++) {
		if (y == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
					"Kilo editor -- version %s", KILO_VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
			} else {
				abAppend(ab, "~", 1);
			}
		abAppend(ab, "\x1b[K", 3);//erase part of line to right of cursor
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);//return and newline
		}
	}
}

void editorRefreshScreen(void){
	struct abuf ab = ABUF_INIT;
	// \x1b is an escape character
	abAppend(&ab, "\x1b[?25l", 6);//hide cursor
	abAppend(&ab, "\x1b[H", 3);//move cursor to 1,1

	editorDrawsRows(&ab);

	//move cursor
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);//show cursor
	
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
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
	E.cx = 0; // column
	E.cy = 0; // row

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
