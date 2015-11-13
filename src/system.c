#include <time.h>
#include <SDL/SDL.h>
#include "common.h"
#include "func.h"

static Pos DecideGoal(Pos pos, int command, int dir);
static int DecideDir(int ct);
static int DecideDistance(Pos p1, Pos p2);
static void Fire(int ct);
static void MakeShot(int ct, int dir);
static Pos MoveChara(Pos pos, Pos startpos, Pos goalpos, int dir);
static void MoveShot();
static int MoveDir(int startdir, int goaldir);
static void CheckWinner();

int restplayer; /* 残り人数 */

/*****************************************************************
関数名 : InitSystem
機能	: 起動時の初期化処理、1回のみ行う処理を実行
引数	: なし
出力	: なし
*****************************************************************/
void InitSystem()
{
	gState = GAME_TITLE;

	gGun[GUN_1SHOT].atk    = 40;
	gGun[GUN_1SHOT].color  = 0xFF0000FF;
	gGun[GUN_3SHOT].atk    = 20;
	gGun[GUN_3SHOT].color  = 0xFF00FFFF;
	gGun[GUN_BUBBLE].atk   = 1;
	gGun[GUN_BUBBLE].color = 0x0000FF80;
	gGun[GUN_MILK].atk     = 1;
	gGun[GUN_MILK].color   = 0xFFFFFFFF;
}

/*****************************************************************
関数名 : InitTitle
機能	: タイトルでの初期化処理
引数	: なし
出力	: なし
*****************************************************************/
void InitTitle()
{
}

/*****************************************************************
関数名 : InitEdit
機能	: 編集での初期化処理
引数	: なし
出力	: なし
*****************************************************************/
void InitEdit()
{
	eState = EDIT_GUN;
	gChara[0].gun = 0;
}

/*****************************************************************
関数名 : InitMain
機能	: メインでの初期化処理
引数	: なし
出力	: なし
*****************************************************************/
void InitMain()
{
    /* 乱数初期化 */
    srand(time(NULL));
	int i, j;

	mState = MAIN_COMMAND;	//コマンド選択画面へ
	cState = COMMAND_DIR;	//方向選択から選択できるように
	CT_NUM = restplayer = MAX_CT;
	 /* 座標, 角度の決定 */
	for(i=0; i<CT_NUM; i++){
		gChara[i].pos.x = i * (WIDTH - S_SIZE) / (CT_NUM - 1);
		gChara[i].pos.y = rand() % (HEIGHT-S_SIZE);
		gChara[i].dir = 0;//rand() % 360;
		gChara[i].state = LIVING;
		gChara[i].maxhp = 150;
		gChara[i].hp = gChara[i].maxhp;
		for(j=0; j<MAX_COMMAND; j++)
			gChara[i].command[j] = -1;
	}

	/* 玉の初期化 */
	for(i=0; i<MAX_SHOT; i++){
		gShot[i].id = -1;
		gShot[i].pos.x = gShot[i].pos.y = gShot[i].dir = 0;
		gShot[i].state = DEAD;
	}

	count = 0;
	nowcommand = 0;
	lastcount = MAX_COUNT;
	gspeed = 1;
	gCommand.dir = 0;		 //上方向から選択できるように
	gCommand.gun = C_SCOPE; //照準から選択できるように

	//COMはとりあえずランダムにコマンドと銃を決定
	for(i=1; i<CT_NUM; i++){
		gChara[i].gun = rand() % MAX_GUN;
		for(j=0; j<MAX_COMMAND; j++){
			///*
			if(j % 2 == 0)
				gChara[i].command[j] = C_SCOPE;
			else
				gChara[i].command[j] = C_FIRE;
			//*/
			//gChara[i].command[j] = rand() % 10;
		}
	}
}

/*****************************************************************
関数名 : UseCommand
機能	: コマンドの適用
引数	: なし
出力	: なし
*****************************************************************/
void UseCommand()
{
	/* カウントを設け、カウントが増えるに伴い各座標も移動する */
	int i, j;

	MoveShot();

	/* 初回 */
	if(count == 0){
		for(i=0; i<CT_NUM; i++){
			if(gChara[i].state == LIVING)
				switch(gChara[i].command[nowcommand]){
				case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: //8方位移動
					gChara[i].startpos = gChara[i].pos;
					gChara[i].goalpos =  DecideGoal(gChara[i].pos, gChara[i].command[nowcommand], gChara[i].dir);
					break;
				case C_SCOPE: //照準
					gChara[i].startdir = (gChara[i].dir % 360);
					gChara[i].goaldir = DecideDir(i) % 360;
					break;
				case C_FIRE: //発射
					Fire(i);
					break;
				default:
					break;
				}
		}
	}
	else if(count <= lastcount){
		for(i=0; i<CT_NUM; i++){
			if(gChara[i].state == LIVING)
				switch(gChara[i].command[nowcommand]){
				case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
						gChara[i].pos = MoveChara(gChara[i].pos, gChara[i].startpos, gChara[i].goalpos, (gChara[i].dir + gChara[i].command[nowcommand] * 45 % 360));
					break;
				case C_SCOPE: //照準
					if(count <= lastcount / 2)
						gChara[i].dir = MoveDir(gChara[i].startdir, gChara[i].goaldir);
					break;
				case C_FIRE: //発射
					for(j=0; j < MAX_COUNT/lastcount; j++)
					if(gChara[i].gun == GUN_BUBBLE || gChara[i].gun == GUN_MILK)
						Fire(i);
					break;
				default:
					break;
				}
		}
	}

	if(count == lastcount){ //カウント終了後のリセット
		count = -1;
		lastcount = MAX_COUNT / gspeed;
		nowcommand++;
		if(nowcommand == MAX_COMMAND)
			nowcommand = 0;
	}

	count++;
}

/*****************************************************************
関数名 : DecideGoal
機能	: キャラクターが移動する際の目的地の座標を求める
引数	: pos : 移動前の座標  command : 8方位の移動方向  dir:キャラクターの角度
出力	: 目的地の座標
*****************************************************************/
Pos DecideGoal(Pos pos, int command, int dir)
{
	Pos goalpos = {0, 0};
	goalpos.x = pos.x + 150 * sin((dir + command * 45) * M_PI /180);
	goalpos.y = pos.y - 150 * cos((dir + command * 45) * M_PI /180);
	return goalpos;
}

/*****************************************************************
関数名 : DecideDir
機能	: 変更後の角度の決定
引数	: ct : 角度を変更するキャラクターの番号
出力	: 変更後の角度
*****************************************************************/
int DecideDir(int ct)
{
	/* 2点間の距離が最も近いキャラクターを決め、それとの角度を求める */
	int i, d[CT_NUM], minid = ct, goaldir = 0;
	for(i=0; i<CT_NUM; i++){
		d[i] = DecideDistance(gChara[ct].pos, gChara[i].pos);
		d[minid] = DecideDistance(gChara[ct].pos, gChara[minid].pos);
		if(((d[minid] > d[i] && d[i] != 0) || d[minid] == 0) && gChara[i].state == LIVING){
			minid = i;
		}
	}
	int dx = gChara[minid].pos.x - gChara[ct].pos.x; //距離の差
	int dy = gChara[minid].pos.y - gChara[ct].pos.y;
	goaldir = (atan2(dy, dx) * 180 / M_PI + 450);
	return goaldir;
}

/*****************************************************************
関数名 : DecideDistance
機能	: 2点間の距離の測定
引数	: p1, p2 : 各キャラクターの座標
出力	: 距離の2乗
*****************************************************************/
int DecideDistance(Pos p1, Pos p2)
{
	int d = 0;
	d = (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
	return d;
}

/*****************************************************************
関数名 : Fire
機能	: 玉の発射処理
引数	: ct : 発射したキャラクターの番号　gun : 武器の種類
出力	: なし
*****************************************************************/
void Fire(int ct)
{
	//int i,j;
	switch(gChara[ct].gun){
	case GUN_1SHOT:
		MakeShot(ct, gChara[ct].dir);
		break;
	case GUN_3SHOT:
		MakeShot(ct, gChara[ct].dir);
		MakeShot(ct, (gChara[ct].dir +       10 ) % 360);
		MakeShot(ct, (gChara[ct].dir + 360 - 10) % 360);
		break;
	case GUN_BUBBLE:
		MakeShot(ct, gChara[ct].dir + rand() % 21 - 15); //2n + 1 - n
		break;
	case GUN_MILK:
		MakeShot(ct, gChara[ct].dir + 10 * sin((double)count / (lastcount/2) * M_PI));
		/*
		j=12;
		for(i=0; i<j; i++)
			MakeShot(ct, count * 9 + i * 360/j);
		*/
		break;
	default:
		break;
	}
}

void MakeShot(int ct, int dir)
{
	int i;
	for(i=0; i<MAX_SHOT; i++)
		if(gShot[i].state == DEAD){
			gShot[i].id = ct;
			gShot[i].state = LIVING;
			gShot[i].pos.x = gChara[ct].pos.x + S_SIZE/2;
			gShot[i].pos.y = gChara[ct].pos.y + S_SIZE/2;
			gShot[i].dir = dir;
			gShot[i].color = gGun[gChara[ct].gun].color;
			break;
		}
}

/*****************************************************************
関数名 : MoveChara
機能	: キャラクターの移動
引数	: pos : 移動前の座標
出力	: なし
*****************************************************************/
Pos MoveChara(Pos pos, Pos startpos, Pos goalpos, int dir)
{
	Pos newpos;
	newpos.x = startpos.x + count * (goalpos.x - startpos.x) / lastcount;
	newpos.y = startpos.y + count * (goalpos.y - startpos.y) / lastcount;
	/* 壁と衝突時 */
	if((newpos.x < 0) || (newpos.x + S_SIZE > WIDTH))
		newpos.x = pos.x;
	if((newpos.y < 0) || (newpos.y + S_SIZE > HEIGHT))
		newpos.y = pos.y;
	return newpos;
}

/*****************************************************************
関数名 : MoveShot
機能	: 先輩こいつ玉とか移動しはじめましたよ
引数	: なし
出力	: なし
*****************************************************************/
void MoveShot()
{
	int i, j, k;
	Pos newpos;
	for(i=0; i<MAX_COUNT/lastcount; i++)
		for(j=0; j<MAX_SHOT; j++)
			if(gShot[j].state == LIVING){
				gShot[j].pos.x = gShot[j].pos.x + 10 * 50 * MAX_COUNT / pow(lastcount, 2) * sin(gShot[j].dir * M_PI /180);
				gShot[j].pos.y = gShot[j].pos.y - 10 * 50 * MAX_COUNT / pow(lastcount, 2) * cos(gShot[j].dir * M_PI /180);
				newpos.x = gShot[j].pos.x;
				newpos.y = gShot[j].pos.y;

				/* 壁と衝突時 */
				if((newpos.x <= 0) || (newpos.x >= WIDTH) || (newpos.y <= 0) || (newpos.y >= HEIGHT)){
					gShot[j].state = DEAD;
				}
				/* 玉とキャラとの衝突時 */
				for(k=0; k<CT_NUM; k++){
					if(k != gShot[j].id && gChara[k].state == LIVING && gShot[j].state == LIVING &&
							newpos.x >= gChara[k].pos.x && newpos.x <= gChara[k].pos.x + S_SIZE &&
							newpos.y >= gChara[k].pos.y && newpos.y <= gChara[k].pos.y + S_SIZE){
						gShot[j].state = DEAD;
						gChara[k].hp -= gGun[gChara[gShot[j].id].gun].atk;
						if( gChara[k].hp <= 0 ){
							gChara[k].state = DEAD;
							restplayer--;
						}
						if(restplayer == 1){
							CheckWinner();
							mState = MAIN_RESULT;
						}
					}
				}
			}
}

/*****************************************************************
関数名 : MoveDir
機能	: 角度の変更
引数	: startdir : 変更を開始する前の角度  goaldir : 目的の角度
出力	: 変更後の角度
*****************************************************************/
int MoveDir(int startdir, int goaldir)
{
	return startdir + (count * (goaldir-startdir) / (lastcount/2)); //(MAXCOUNT / n)でn倍速く回転
}

void CheckWinner()
{
	int i;
	for(i=0; i<CT_NUM; i++)
		if(gChara[i].state == LIVING)
			winner = i;
}

/* end of system.c */
