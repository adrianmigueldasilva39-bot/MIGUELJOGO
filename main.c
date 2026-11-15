#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <io/pad.h>
#include <tiny3d.h>
#define WORLD_R 1500.0f
#define MAX_SNAKES 7
#define MAX_FOOD 140
#define MAX_SEG 60
#define W 1280
#define H 720
typedef struct{float x;float y;}Vec2;
typedef struct{float x;float y;int color;}Food;
typedef struct{
Vec2 segs[MAX_SEG];
int len;
float angle;
float tang;
float score;
int alive;
int isPlayer;
int colorIdx;
float rad;
float aiTimer;
char name[16];
}Snake;
static Snake snakes[MAX_SNAKES];
static Food foods[MAX_FOOD];
static int foodCount=0;
static float camX=0;
static float camY=0;
static int gameOver=0;
static u32 colors[5][2]={
{0xFF00E676,0xFF00C853},
{0xFFFF4081,0xFFF50057},
{0xFF2979FF,0xFF2962FF},
{0xFFFFEA00,0xFFFFD600},
{0xFFAA00FF,0xFF7B1FA2}
};
static u32 foodColors[5]={0xFFFF5252,0xFFFFEA00,0xFF00E676,0xFF2979FF,0xFFFF4081};
static float frand(float a,float b){return a+(float)rand()/(float)RAND_MAX*(b-a);}
void spawnFood(int n){
for(int i=0;i<n&&foodCount<MAX_FOOD;i++){
float a=frand(0,6.283f);
float r=sqrtf(frand(0,1))*(WORLD_R-80);
foods[foodCount].x=cosf(a)*r;
foods[foodCount].y=sinf(a)*r;
foods[foodCount].color=rand()%5;
foodCount++;
}
}
void initSnake(int idx,int isPlayer,int ci,float x,float y,const char*name){
Snake*s=&snakes[idx];
memset(s,0,sizeof(Snake));
s->angle=frand(0,6.28f);
s->tang=s->angle;
s->len=9;
s->rad=10;
s->alive=1;
s->isPlayer=isPlayer;
s->colorIdx=ci;
strncpy(s->name,name,15);
for(int i=0;i<s->len;i++){
s->segs[i].x=x-cosf(s->angle)*i*10;
s->segs[i].y=y-sinf(s->angle)*i*10;
}
if(!isPlayer){
s->len=12;
s->score=60;
for(int i=9;i<s->len;i++){s->segs[i]=s->segs[9];}
}
}
void resetGame(){
foodCount=0;
spawnFood(MAX_FOOD);
initSnake(0,1,0,0,0,"Player");
const char*bots[6]={"Viper","Kobra","Mamba","Rex","Ghost","Zeus"};
for(int i=0;i<6;i++){
float a=frand(0,6.28f);
float r=frand(300,WORLD_R-400);
initSnake(i+1,0,i%5,cosf(a)*r,sinf(a)*r,bots[i]);
}
camX=0;
camY=0;
gameOver=0;
}
void killSnake(int idx){
Snake*s=&snakes[idx];
if(!s->alive)return;
s->alive=0;
for(int i=0;i<s->len;i+=4){
if(foodCount<MAX_FOOD){
foods[foodCount].x=s->segs[i].x+frand(-8,8);
foods[foodCount].y=s->segs[i].y+frand(-8,8);
foods[foodCount].color=s->colorIdx;
foodCount++;
}
}
if(s->isPlayer)gameOver=1;
}
void updateGame(float dt,float moveAngle,int boost){
for(int si=0;si<MAX_SNAKES;si++){
Snake*s=&snakes[si];
if(!s->alive)continue;
if(s->isPlayer){
float diff=fmodf(moveAngle-s->angle+9.42f,6.28f)-3.14f;
s->angle+=diff*fminf(1,dt*7);
}else{
s->aiTimer-=dt;
if(s->aiTimer<=0){
s->aiTimer=0.35f;
Vec2 h=s->segs[0];
float bestD=1e9;
Food*best=NULL;
for(int i=0;i<foodCount;i+=3){
float dx=foods[i].x-h.x;
float dy=foods[i].y-h.y;
float dd=dx*dx+dy*dy;
if(dd<62500&&dd<bestD){bestD=dd;best=&foods[i];}
}
if(best)s->tang=atan2f(best->y-h.y,best->x-h.x);
else s->tang+=frand(-1,1);
float dc=sqrtf(h.x*h.x+h.y*h.y);
if(dc>WORLD_R-250)s->tang=atan2f(-h.y,-h.x);
}
float diff=fmodf(s->tang-s->angle+9.42f,6.28f)-3.14f;
s->angle+=diff*fminf(1,dt*4);
}
int isBoost=(s->isPlayer?boost:0);
float spd=(isBoost&&s->len>12)?4.1f:2.15f;
Vec2 h=s->segs[0];
Vec2 nh={h.x+cosf(s->angle)*spd*60*dt,h.y+sinf(s->angle)*spd*60*dt};
for(int i=s->len-1;i>0;i--)s->segs[i]=s->segs[i-1];
s->segs[0]=nh;
for(int i=1;i<s->len;i++){
Vec2 p=s->segs[i-1];
Vec2 c=s->segs[i];
float dx=c.x-p.x;
float dy=c.y-p.y;
float d=sqrtf(dx*dx+dy*dy);
if(d>9&&d>0.001f){s->segs[i].x=p.x+dx/d*9;s->segs[i].y=p.y+dy/d*9;}
}
for(int i=foodCount-1;i>=0;i--){
float dx=h.x-foods[i].x;
float dy=h.y-foods[i].y;
if(dx*dx+dy*dy<324){
foods[i]=foods[foodCount-1];
foodCount--;
s->score+=1;
s->len+=1;
if(s->len>MAX_SEG)s->len=MAX_SEG;
s->rad=fminf(15,9+s->len*0.08f);
}
}
if(nh.x*nh.x+nh.y*nh.y>(WORLD_R-10)*(WORLD_R-10)){killSnake(si);continue;}
for(int o=0;o<MAX_SNAKES;o++){
if(!snakes[o].alive)continue;
int st=(o==si)?8:0;
for(int i=st;i<snakes[o].len;i+=3){
float dx=nh.x-snakes[o].segs[i].x;
float dy=nh.y-snakes[o].segs[i].y;
if(dx*dx+dy*dy<90){killSnake(si);break;}
}
if(!s->alive)break;
}
}
if(snakes[0].alive){
camX+=(snakes[0].segs[0].x-camX)*fminf(1,dt*5);
camY+=(snakes[0].segs[0].y-camY)*fminf(1,dt*5);
}
if(foodCount<120)spawnFood(25);
}
void drawCircle(float x,float y,float r,u32 col){
tiny3d_SetPolygon(TINY3D_TRIANGLE_FAN);
tiny3d_VertexPos(x,y,0);
tiny3d_VertexColor(col);
for(int i=0;i<=16;i++){
float a=i/16.0f*6.283f;
tiny3d_VertexPos(x+cosf(a)*r,y+sinf(a)*r,0);
tiny3d_VertexColor(col);
}
tiny3d_End();
}
int main(){
tiny3d_Init(1024*1024);
ioPadInit(7);
srand(time(NULL));
resetGame();
padData pad;
float moveAngle=0;
struct timeval last,now;
gettimeofday(&last,NULL);
while(1){
gettimeofday(&now,NULL);
float dt=(now.tv_sec-last.tv_sec)+(now.tv_usec-last.tv_usec)/1000000.0f;
last=now;
if(dt>0.033f)dt=0.016f;
if(dt<0.001f)dt=0.016f;
ioPadGetData(0,&pad);
int boost=0;
if(pad.len>0){
int ax=pad.ANA_L_H-128;
int ay=pad.ANA_L_V-128;
if(ax*ax+ay*ay>400)moveAngle=atan2f((float)ay,(float)ax);
if(pad.BTN_CROSS||pad.BTN_R2)boost=1;
if(pad.BTN_START&&gameOver)resetGame();
}
if(!gameOver)updateGame(dt,moveAngle,boost);
tiny3d_Clear(0xFF12121E,TINY3D_CLEAR_ALL);
tiny3d_Project2D();
float camL=camX-W/2;
float camT=camY-H/2;
tiny3d_SetPolygon(TINY3D_LINE_LOOP);
for(int i=0;i<64;i++){
float a=i/64.0f*6.283f;
tiny3d_VertexPos(cosf(a)*WORLD_R-camL,sinf(a)*WORLD_R-camT,0);
tiny3d_VertexColor(0xFFFF1744);
}
tiny3d_End();
for(int i=0;i<foodCount;i++){
drawCircle(foods[i].x-camL,foods[i].y-camT,4,foodColors[foods[i].color]);
}
for(int si=0;si<MAX_SNAKES;si++){
Snake*s=&snakes[si];
if(!s->alive)continue;
for(int i=s->len-1;i>=0;i--){
float rad=s->rad*(1-(float)i/s->len*0.4f);
u32 c=colors[s->colorIdx][(i>>2)&1];
drawCircle(s->segs[i].x-camL,s->segs[i].y-camT,rad,c);
}
Vec2 h=s->segs[0];
float ex=cosf(s->angle);
float ey=sinf(s->angle);
float px=-ey;
float py=ex;
for(int sd=-1;sd<=1;sd+=2){
drawCircle(h.x+ex*5+px*sd*6-camL,h.y+ey*5+py*sd*6-camT,s->rad*0.38f,0xFFFFFFFF);
drawCircle(h.x+ex*7+px*sd*6-camL,h.y+ey*7+py*sd*6-camT,s->rad*0.2f,0xFF000000);
}
}
tiny3d_Flip();
}
return 0;
}