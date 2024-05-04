#include <bits/types/struct_timeval.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK "█"
#define DELAY_MS 100
#define WINDOW_WIDTH 84
#define WINDOW_HEIGHT 21

typedef struct block {
	int x,y;
	int w, h;
} block_t;

typedef struct ball {
	int xsp, ysp;
	block_t b;
} ball_t;

static ball_t ball;
static block_t baffle1, baffle2;
static WINDOW *game_window;
static WINDOW *menu_window;
static int gx, gy, gw, gh;
static int mx, my, mw, mh;
static int state = 1;
static unsigned int score[2] = {0, 0};
static pthread_t update_thread;
static pthread_mutex_t state_lock, baffle1_lock, 
		       baffle2_lock, score1_lock,
		       score2_lock;

void 
draw_block(block_t b)
{
	for (int i = 0; i < b.h * b.w; i++)
		mvwaddstr(game_window, b.y + i / b.w, b.x + i % b.w, BLOCK);
}

void
draw_border(WINDOW *win, int w, int h)
{
	
	mvwaddstr(win, 0, 0,"╭");
	mvwaddstr(win, h - 1, 0,"╰");
	mvwaddstr(win, 0, w - 1,"╮");
	mvwaddstr(win, h - 1, w - 1,"╯");
	int i;
	for (i = 1; i < w - 1; i++) {
		mvwaddstr(win, 0, i, "─");
		mvwaddstr(win, h - 1, i, "─");
	}
	for (i = 1; i < h - 1; i++) {
		mvwaddstr(win,i, 0, "│");
		mvwaddstr(win,i, w - 1, "│");
	}
}

void
add(int *a, int n, int max)
{
	if (n >= 0 && *a + n < max)
		*a += n;
	else
		*a = max;
}


void
sub(int *a, int n, int min)
{
	if ( n >= 0 && *a - n > min)
		*a -= n;
	else
		*a = min;
}

#define block_add_x(b, n) add(&b.x, n, gw - b.w - 1)
#define block_sub_x(b, n) sub(&b.x, n, 1)
#define block_add_y(b, n) add(&b.y, n, gh - b.h - 1)
#define block_sub_y(b, n) sub(&b.y, n, 1)

int isrunning(void)
{
	pthread_mutex_lock(&state_lock);
	if (state == 1) {
		pthread_mutex_unlock(&state_lock);
		return 1;
	} else if (state == 2) {
		pthread_mutex_unlock(&state_lock);
		return 2;
	} else {
		pthread_mutex_unlock(&state_lock);
		return 0;
	}
}

void * update(void *);

void start(void)
{
	pthread_mutex_lock(&state_lock);
	state = 1;
	pthread_mutex_unlock(&state_lock);
}

void stop(void)
{
	pthread_mutex_lock(&state_lock);
	state = 2;
	pthread_mutex_unlock(&state_lock);

}

void end(void)
{
	pthread_mutex_lock(&state_lock);
	state = 0;
	pthread_mutex_unlock(&state_lock);
}

void
is_touch_boundary(void)
{
	if (ball.b.x == gw - ball.b.w - 1){
		ball.b.x = gw / 2;
		ball.b.y = gh / 2;
		ball.ysp = -ball.ysp;
		pthread_mutex_lock(&score1_lock);
		score[0]++;
		pthread_mutex_unlock(&score1_lock);
	} else if( ball.b.x == 1) {
		ball.b.x = gw / 2;
		ball.b.y = gh / 2;
		ball.ysp = -ball.ysp;
		pthread_mutex_lock(&score2_lock);
		score[1]++;
		pthread_mutex_unlock(&score2_lock);
	}
	if (ball.b.y == gh - ball.b.h - 1) {
		ball.ysp = -1;
	} else if (ball.b.y == 1) {
		ball.ysp = 1;
	}
}

#define BETWEEN(x, a, b) (x<= b && x >= a)

void
is_touch_baffle(void)
{
	int bax = ball.b.x + ball.xsp,
	    bay = ball.b.y + ball.ysp,
	    b1x = baffle1.x + baffle1.w - 1,
	    b1bot = baffle1.y,
	    b1top = baffle1.y + baffle1.h - ball.b.h,
	    b2x = baffle2.x - ball.b.w + 1,
	    b2bot = baffle2.y,
	    b2top = baffle2.y + baffle2.h - ball.b.h;

	if ((bax == b1x) && BETWEEN(bay, b1bot, b1top)) {
		ball.xsp = -ball.xsp;
		ball.ysp = bay - b1bot > 1 ? (bay - b1bot > 3 ? 1 : 0) : -1;
	}
	else if ((bax == b2x) && BETWEEN(bay, b2bot, b2top)) {
		ball.xsp = -ball.xsp;
		ball.ysp = bay - b2bot > 3 ? 1 : -1;
		ball.ysp = bay - b2bot > 1 ? (bay - b2bot > 3 ? 1 : 0) : -1;
	}
}

int
game_update(int state)
{
	if (state == 2)
		goto stop;

	pthread_mutex_lock(&baffle1_lock);
	pthread_mutex_lock(&baffle2_lock);
	
	wclrtobot(game_window);
	wclrtobot(menu_window);
	is_touch_boundary();
	is_touch_baffle();
	switch (ball.xsp) {
		case 0:
			break;
		case 1:
			block_add_x(ball.b, 1);
			break;
		case -1:
			block_sub_x(ball.b, 1);
			break;
		default:
			return 1;
	}
	
	switch (ball.ysp) {
		case 0:
			break;
		case 1:
			block_add_y(ball.b, 1);
			break;
		case -1:
			block_sub_y(ball.b, 1);
			break;
		default:
			return 1;
	}
stop:
	draw_block(ball.b);
	draw_block(baffle1);
	draw_block(baffle2);
	draw_border(game_window, gw, gh);
	wmove(game_window , 0, 2);
	wprintw(game_window, "GAME");
	if (isrunning() == 1)
		wprintw(game_window, "(Running)");
	else
		wprintw(game_window, "(Stop)");
	wrefresh(game_window);
	pthread_mutex_unlock(&baffle1_lock);
	pthread_mutex_unlock(&baffle2_lock);
	
	draw_border(menu_window, mw, mh);
	wmove(menu_window , 0, 2);
	wprintw(menu_window, "MENU");
	
	wmove(menu_window , 2, mw / 4 + 4);
	wprintw(menu_window, "SCORE");
	
	pthread_mutex_lock(&score1_lock);
	pthread_mutex_lock(&score2_lock);
	wmove(menu_window , 3, mw / 4);
	wprintw(menu_window, "Player1 : %d", score[0]);
	wmove(menu_window , 4, mw / 4);
	wprintw(menu_window, "Player2 : %d", score[1]);
	pthread_mutex_unlock(&score1_lock);
	pthread_mutex_unlock(&score2_lock);

	wmove(menu_window , mh - 8, mw / 4);
	wprintw(menu_window, "	MANUAL");
	wmove(menu_window , mh - 7, mw / 4);
	wprintw(menu_window, "'w','j'----up");
	wmove(menu_window , mh - 6, mw / 4);
	wprintw(menu_window, "'s','k'--down");
	wmove(menu_window , mh - 5, mw / 4);
	wprintw(menu_window, "'p'------stop");
	wmove(menu_window , mh - 4, mw / 4);
	wprintw(menu_window, "'r'-----start");
	wmove(menu_window , mh - 3, mw / 4);
	wprintw(menu_window, "'q'------exit");
	wrefresh(menu_window);
	return 0;
}

void
msleep(int ms)
{
	struct timeval delay;
	delay.tv_sec = ms / 1000;
	delay.tv_usec = (ms % 1000) * 1000;
	select(0, NULL, NULL, NULL, &delay);
}

void *
update(void * null)
{
	int nstate;
	while((nstate = isrunning())) {
		if (game_update(nstate))
			pthread_exit(NULL);
		msleep(DELAY_MS);
	}
	return NULL;
}

void
game_init(void)
{
	setlocale(LC_ALL, "en_US.UTF-8");
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	gx = 1, gy = 1, gw = (WINDOW_WIDTH - 1) / 4 * 3 - 1, gh = WINDOW_HEIGHT - 2;
	game_window = newwin(gh, gw, gy, gx);
	mx = (WINDOW_WIDTH - 1) / 4 * 3, my = 1, mw = WINDOW_WIDTH - mx - 1, mh = WINDOW_HEIGHT - 2;
	menu_window = newwin(mh, mw, my, mx);
		
	draw_border(stdscr, WINDOW_WIDTH, WINDOW_HEIGHT);
	move(0, 2);
	printw("Ping-Pong");
	refresh();
	ball.ysp = 1;
	ball.xsp = 1;
	ball.b.x = gw / 2;
	ball.b.y = gh / 2;
	ball.b.w = 2;
	ball.b.h = 1;

	baffle1.h = 6;
	baffle1.w = 2;
	baffle1.x = 4;
	baffle1.y = gh / 2 - 3;
	
	baffle2.h = 6;
	baffle2.w = 2;
	baffle2.x = gw - 6;
	baffle2.y = gh / 2 - 3;
	
	draw_border(game_window, gw, gh);
	wrefresh(game_window);
	draw_border(menu_window, mw, mh);
	wrefresh(menu_window);

	pthread_mutex_init(&baffle1_lock, NULL);
	pthread_mutex_init(&baffle2_lock, NULL);
	pthread_mutex_init(&state_lock, NULL);
	pthread_mutex_init(&score1_lock, NULL);
	pthread_mutex_init(&score2_lock, NULL);
}

void
game_exit(void)
{
	end();
	pthread_join(update_thread, NULL);
	pthread_mutex_destroy(&baffle1_lock);
	pthread_mutex_destroy(&baffle2_lock);
	pthread_mutex_destroy(&state_lock);
	pthread_mutex_destroy(&score1_lock);
	pthread_mutex_destroy(&score2_lock);
	endwin();
	exit(0);
}

int
main(int argc, char *argv[])
{
	game_init();
	start();
	pthread_create(&update_thread, NULL, update, NULL);
	while(isrunning()) {
		switch(getch()) {
			case 'w':
				pthread_mutex_lock(&baffle1_lock);
				block_sub_y(baffle1, 1);
				pthread_mutex_unlock(&baffle1_lock);
				break;
			case 's':
				pthread_mutex_lock(&baffle1_lock);
				block_add_y(baffle1, 1);
				pthread_mutex_unlock(&baffle1_lock);
				break;
			case 'j':
				pthread_mutex_lock(&baffle2_lock);
				block_sub_y(baffle2, 1);
				pthread_mutex_unlock(&baffle2_lock);
				break;
			case 'k':
				pthread_mutex_lock(&baffle2_lock);
				block_add_y(baffle2, 1);
				pthread_mutex_unlock(&baffle2_lock);
				break;
			case 'p':
				stop();
				break;
			case 'r':
				start();
				break;
			case 'q':
				end();
				break;
			default:
				break;
		}
	}	
	game_exit();
}
