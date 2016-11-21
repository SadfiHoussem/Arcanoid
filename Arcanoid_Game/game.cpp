#include "stdafx.h"
#include "svga/svga.h"
#include "time.h"
#include "windows.h"
#include <string>
#include <vector>


#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
//This function update full screen from scrptr. The array should be at least sv_height*scrpitch bytes size;
void w32_update_screen(void *scrptr,unsigned scrpitch);

//If this variable sets to true - game will quit

extern bool game_quited;

// these variables provide access to joystick and joystick buttons
// In this version joystick is simulated on Arrows and Z X buttons

// [0]-X axis (-501 - left; 501 - right)
// [1]-Y axis (-501 - left; 501 - right)
extern int gAxis[2];
//0 - not pressed; 1 - pressed
extern int gButtons[6];

//sv_width and sv_height variables are width and height of screen
extern unsigned int sv_width,sv_height;

//These functions called from another thread, when a button is pressed or released
void win32_key_down(unsigned k){
  if(k==VK_F1) game_quited=true;
}
void win32_key_up(unsigned){}


typedef struct {
	size_t width;
	size_t height;
	int x;
	int y;
} Paddle;

static Paddle *
paddle_new(size_t width,
	size_t height, int x, int y)
{
	Paddle *paddle;

	paddle = (Paddle *)malloc(sizeof *paddle);
	paddle->width = width;
	paddle->height = height;
	paddle->x = x;
	paddle->y = y;

	return paddle;
}

typedef struct {
	int radius;
	int x;
	int y;
	int vx;	// velocity
	int vy;
	bool actif;
} Circle;

static Circle *
circle_new(int radius, int x, int y)
{
	Circle *circle;

	circle = (Circle *)malloc(sizeof *circle);
	circle->radius = radius;
	circle->x = x;
	circle->y = y;
	circle->vx = -1;
	circle->vy = -1;
	circle->actif = false;

	return circle;
}

typedef struct {
	size_t width;
	size_t height;
	bool destroyed;
	int x;
	int y;
} Brick;

static Brick *
brick_new(size_t width,
	size_t height, int x, int y)
{
	Brick *brick;

	brick = (Brick *)malloc(sizeof *brick);
	brick->width = width;
	brick->height = height;
	brick->destroyed = false;
	brick->x = x;
	brick->y = y;

	return brick;
}

//This is default fullscreen shadow buffer. You can use it if you want to.
static unsigned *shadow_buf=NULL;

Paddle* mPaddle;
std::vector<Brick*> mbricks;
std::vector<Circle*> mballs;

void delay(unsigned int mseconds)
{
	clock_t goal = mseconds + clock();
	while (goal > clock());
}

static void createBalls(int numBalls,
	int radius)
{
	int varX = 0;
	int varY = 0;
	for (int i = 0; i < numBalls; i++) {
		Circle* ball;
		ball = circle_new(radius, ((sv_width / 2) + varX), ((sv_height / 2) + varY));
		mballs.push_back(ball);
		varX = (varX + radius * 3) % sv_width;
		varY = (varY + radius * 3) % sv_height;
	}
}

static void activeBall(int numBall) {
	if ((numBall<mballs.size())&&(numBall>=0)) {
		mballs.at(numBall)->actif = true;
	}
}

static void 
moveBall(Circle* ball) {
	ball->x = ball->x + ball->vx;
	ball->y = ball->y + ball->vy;
}

static void 
moveBalls() {
	for (auto& ball : mballs) {
		if (ball->actif) {
			moveBall(ball);
		}
	}
}

static void
circle_set_pixel(unsigned *shadow_buf,
	Circle* ball,
	ssize_t        x,
	ssize_t        y,
	unsigned value)
{
	size_t tx, ty;
	unsigned *p;
	
	tx = ball->x + x;
	ty = ball->y + y;

	memset(&(shadow_buf[(ty * sv_width) + tx]), value, 1);

	if ((tx + ball->vx) < 0) {
		ball->vx = ball->vx * -1;
	}
	if ((tx + ball->vx) > sv_width) {
		ball->vx = ball->vx * -1;
	}
	if ((ty + ball->vy) > sv_height) {
		game_quited = true;
		ball->vy = ball->vy * -1;
	}
	if ((ty + ball->vy) < 45) {
		ball->vy = ball->vy * -1;
	}
}

static void
drawBalls(unsigned *shadow_buf,
	unsigned value)
{
	int x, y;
	for (auto& ball : mballs) {
		if (ball->actif) {
			for (y = -(ball->radius); y <= ball->radius; y++)
				for (x = -(ball->radius); x <= ball->radius; x++)
					if ((x * x) + (y * y) <= (ball->radius * ball->radius))
						circle_set_pixel(shadow_buf, ball, x, y, value);
		}
	}
}

static void
movePaddle(ssize_t x) {
	int currentMove = gAxis[0] != 0 ? (gAxis[0] / gAxis[0]) * 5 : 0;
	if ((gAxis[0] > 0)&&((mPaddle->x + currentMove + x) < sv_width)) {
		mPaddle->x = mPaddle->x + currentMove;
	}
	if ((gAxis[0] < 0) && ((mPaddle->x - currentMove ) > 0)) {
		mPaddle->x = mPaddle->x - currentMove;
	}
}

static void
paddle_set_pixel(unsigned *shadow_buf,
	Paddle* paddle,
	ssize_t        x,
	ssize_t        y,
	unsigned value)
{
	size_t tx, ty;
	
	tx = paddle->x + x; 
	ty = paddle->y + y;
	
	memset(&(shadow_buf[(ty * sv_width) + tx]), value, 1);
	//rand() % 255

}

static void
drawPaddle(unsigned *shadow_buf,
Paddle* paddle,
unsigned value)
{
	int x, y;
	for (y = 0; y <= paddle->height; y++)
		for (x = 0; x <= paddle->width; x++)
			paddle_set_pixel(shadow_buf,paddle, x, y, value);
}

static void createPaddle(size_t height,
	size_t width, int x, int y)
{
	mPaddle = paddle_new(width, height, x, y);
}

static void
brick_set_pixel(unsigned *shadow_buf,
	ssize_t        x,
	ssize_t        y,
	unsigned value)
{
	size_t tx, ty;
	
	tx = x;
	ty = y;

	memset(&(shadow_buf[(ty * sv_width) + tx]), value, 1);

}

static void
drawBrick(unsigned *shadow_buf,
	int length,
	int width,
	int xPos,
	int yPos,
	unsigned value)
{
	int x, y;
	for (y = yPos; y <= (length + yPos); y++)
		for (x = xPos; x <= (width + xPos); x++)
			brick_set_pixel(shadow_buf, x, y, value);
}

static void 
drawBricks(unsigned *shadow_buf)
{
	for (auto& brick : mbricks) {
		if(!brick->destroyed) drawBrick(shadow_buf, brick->height, brick->width, brick->x, brick->y, 155);
	}
}

static void 
createBricks(int numRow, 
	int distanceBetweenBricks,
	int brickLength, int brickWidth)
{
	int brickPositionX = 10;
	int brickPositionY = distanceBetweenBricks + 45; // 45 is the minimum height for the brick to be visible on the window

	for (int i = 1; i <= numRow; i++) {

		while ((brickPositionX + brickWidth) < sv_width) {
			Brick *brick;
			brick = brick_new(brickWidth,brickLength, brickPositionX, brickPositionY);
			mbricks.push_back(brick);
			brickPositionX = brickPositionX + brickWidth + distanceBetweenBricks;
		}
		brickPositionX = 10;
		brickPositionY = brickPositionY + distanceBetweenBricks + brickLength;
	}
}

static bool 
checkCollide(Paddle *paddle, Circle *circle)
{
	// AABB 1
	int x1Min = paddle->x - 10;
	int x1Max = paddle->x + paddle->width + 10;
	int y1Max = paddle->y + paddle->height;
	int y1Min = paddle->y;

	// AABB 2
	int x2Min = circle->x - circle->radius;
	int x2Max = circle->x + circle->radius;
	int y2Max = circle->y + circle->radius;
	int y2Min = circle->y - circle->radius;

	// Collision tests
	if (x1Max < x2Min || x1Min > x2Max) {
		return false;
	}
	if (y1Max < y2Min || y1Min > y2Max) {
		return false;
	}

	return true;
}

static bool
checkCollide(Brick *brick, Circle *circle)
{
	// AABB 1
	int x1Min = brick->x;
	int x1Max = brick->x + brick->width;
	int y1Max = brick->y + brick->height;
	int y1Min = brick->y;

	// AABB 2
	int x2Min = circle->x - circle->radius;
	int x2Max = circle->x + circle->radius;
	int y2Max = circle->y + circle->radius;
	int y2Min = circle->y - circle->radius;

	// Collision tests
	if (x1Max < x2Min || x1Min > x2Max) {
		return false;
	}
	if (y1Max < y2Min || y1Min > y2Max) {
		return false;
	}

	return true;
}

static void 
testCollisionPaddleBall(Paddle* paddle, Circle* ball) {
	if(!checkCollide(paddle, ball)) return;
	
	// Push the ball up
	ball->vy = ball->vy * -1;
	if ((ball->x - ball->radius) < paddle->x) {
		if(ball->vx > 0) ball->vx = ball->vx * -1;
	}
	else {
		if (ball->vx < 0) ball->vx = ball->vx * -1;
	}
}

static void
testCollisionPaddleBalls() {
	for (auto& ball : mballs) {
		if (ball->actif) {
			testCollisionPaddleBall(mPaddle, ball);
		}
	}
}

static void
testCollisionBrickBall(Brick* brick, Circle* ball) {
	if (!checkCollide(brick, ball)) return;
	
	if (!brick->destroyed) {
		brick->destroyed = true;
		// AABB 1
		int mBrickLeft = brick->x;
		int mBrickRight = brick->x + brick->width;
		int mBrickTop = brick->y + brick->height;
		int mBrickBottom = brick->y;

		// AABB 2
		int mBallLeft = ball->x - ball->radius;
		int mBallRight = ball->x + ball->radius;
		int mBallTop = ball->y + ball->radius;
		int mBallBottom = ball->y - ball->radius;

		int overlapLeft{ mBallRight - mBrickLeft };
		int overlapRight{ mBrickRight - mBallLeft };
		int overlapTop{ mBallBottom - mBrickTop };
		int overlapBottom{ mBrickBottom - mBallTop };

		bool ballFromLeft(abs(overlapLeft) < abs(overlapRight));
		bool ballFromTop(abs(overlapTop) < abs(overlapBottom));

		int minOverlapX{ ballFromLeft ? overlapLeft : overlapRight };
		int minOverlapY{ ballFromTop ? overlapTop : overlapBottom };

		if (abs(minOverlapX) < abs(minOverlapY)) {
			if ((ballFromLeft)&&(ball->vx>0)) {
				ball->vx = -(ball->vx);
			}
			if ((!ballFromLeft) && (ball->vx<0)) {
				ball->vx = -(ball->vx);
			}
		}
		else
		{
			if ((ballFromTop) && (ball->vy<0)) {
				ball->vy = -(ball->vy);
			}
			if ((!ballFromTop) && (ball->vy>0)) {
				ball->vy = -(ball->vy);
			}
		}
	}
}

static void
testCollisionBricksBall(Circle* ball) {
	for (auto& brick : mbricks) {
		testCollisionBrickBall(brick,ball);
	}
}

static void
testCollisionBricksBalls() {
	for (auto& ball : mballs) {
		testCollisionBricksBall(ball);
	}
}

//draw the game to screen
void draw_game(){
  if(!shadow_buf)return;
  
  memset(shadow_buf,0,sv_width*sv_height*4);
  
  drawBricks(shadow_buf);
  drawPaddle(shadow_buf, mPaddle, 255); // 255 Color
  movePaddle(30);
  drawBalls(shadow_buf, 255);
  moveBalls();
  testCollisionPaddleBalls();
  testCollisionBricksBalls();
  delay(10);
  
  
  //here you should draw anything you want in to shadow buffer. (0 0) is left top corner
  w32_update_screen(shadow_buf,sv_width*4);


}

//act the game. dt - is time passed from previous act
void act_game(float dt) {
}

void init_game() {
	shadow_buf = new unsigned[sv_width*sv_height];
	createPaddle(10, 30, sv_width/2, sv_height - 60);
	createBricks(3, 10, 20, 30); // Create 3 rows of bricks (lenght: 20, width:30) and seperated by 10 pixel from each other
	createBalls(2,10); // Create 2 Balls with 10 pixel radius
	activeBall(0); // Active ball number 0
	//activeBall(1);
}

void close_game() {
	if (shadow_buf) delete shadow_buf;
	shadow_buf = NULL;
}
