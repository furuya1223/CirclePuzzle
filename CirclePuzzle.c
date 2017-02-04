#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <GL/glut.h>

#define WND_WIDTH 480	// ウィンドウ横幅
#define WND_HEIGHT 640	// ウィンドウ縦幅
#define HORIZONTAL 12	// フィールドの横サイズ（デフォルトは12。最小は3）
#define VERTICAL 20		// フィールドの縦サイズ（デフォルトは20。8くらいでも十分遊べるが高難度）
//#define RADIUS 1.5	// フィールドの円の半径（定義しなければHORIZONTAL/8.0になる）
#define MARGIN 0.05		// フィールドのブロック間の隙間（見映え用）

#ifndef RADIUS
#define RADIUS (HORIZONTAL / 8.0)
#endif

// 光源の位置と色
const GLfloat lightPos[] = {-10, 20, 10, 0};
const GLfloat lightCol[] = {1.0, 1.0, 1.0, 1.0};

// 2次元整数ベクトル
typedef struct{
	int x;
	int y;
}vec2i;

// 3次元実数ベクトル
typedef struct{
	float x;
	float y;
	float z;
}vec3f;

int right_click = 0;				// 右ドラッグフラグ
vec2i motion_prev;					// 直前のカーソル位置
vec3f eye = {0.0, 10.0, 34.0};		// 視点
vec3f lookat = {0.0, 10.0, 0.0};	// 注視点
vec3f look = {0.0, 0.0, -1.0};		// 視線方向単位ベクトル
vec3f up = {0.0, 1.0, 0.0};			// 上方向単位ベクトル
float look_dist = 34.0;				// 視点から注視点までの距離

int ctrl = 0;						// CTRLキー押下フラグ
int shift = 0;						// SHIFTキー押下フラグ

// -------------------------------------------------------------------
// パズル実装---------------------------------------------------------

// テトリミノの各ブロックの相対位置
typedef struct{
	int x;
	int y;
}POSITION;

// 各種ブロックの情報
typedef struct{
	int rotate;
	POSITION p[3];
}BLOCK;

// 操作中のテトリミノの状態
typedef struct{
	int x;
	int y;
	int type;
	int rotate;
}STATUS;

int board[HORIZONTAL][VERTICAL + 2];	// テトリスフィールド（上下に番兵）
STATUS current;							// 現在操作中のテトリミノの状態
int score = 0;				// スコア。ライン消しと上キー落下で増加
int level = 0;				// レベル。テトリミノ出現とラインけしで増加
int time_interval = 1000;	// 落下する時間間隔。単位はミリ秒
int game_over = 0;			// ゲームオーバーフラグ

// ブロックの種類。何もないところはNULLブロック
BLOCK block[9] = {
	{1, {{0, 0}, {0, 0}, {0, 0}}},		// NULL
	{1, {{0, 0}, {0, 0}, {0, 0}}},		// 壁
	{2, {{0, -1}, {0, 1}, {0, 2}}},		// I
	{4, {{0, -1}, {0, 1}, {1, 1}}},		// L
	{4, {{0, -1}, {0, 1}, {-1, 1}}},	// J
	{2, {{0, -1}, {1, 0}, {1, 1}}},		// S
	{2, {{0, -1}, {-1, 0}, {-1, 1}}},	// Z
	{1, {{0, 1}, {1, 0}, {1, 1}}},		// O
	{4, {{0, -1}, {1, 0}, {-1, 0}}}		// T
};

// ブロックの色。公式準拠。
float block_color[9][4] = {
	{0.0, 0.0, 0.0, 1.0},	// 無し
	{0.6, 0.6, 0.6, 1.0},	// 壁
	{0.0, 0.6, 0.6, 1.0},	// I
	{0.6, 0.4, 0.0, 1.0},	// L
	{0.0, 0.0, 0.6, 1.0},	// J
	{0.0, 0.6, 0.0, 1.0},	// S
	{0.6, 0.0, 0.0, 1.0},	// Z
	{0.6, 0.6, 0.0, 1.0},	// O
	{0.6, 0.0, 0.6, 1.0}	// T
};

// 0からn-1までの乱数を返す関数
int randN(int n){
	return (int)((rand() / ((double)RAND_MAX + 1.0)) * n);
}

// レベルアップ関数
void levelUp(){
	if(level < 999){
		level++;
		// 10レベルごとに少しづつ落下速度増加
		time_interval = 1000 - (level / 10) * 10;
	}
}

// 新たなブロックを出現させる関数
int createBlock(){
	static int counter = 7;
	static int next[7];
	int i, j, n, flag = 1;
	
	// 1周したら新たに次の7つのブロックを配備
	if(counter == 7){
		for(i = 0; i < 7; i++){
			n = randN(7) + 2;		// NULLと壁以外のブロックを用意
			for(j = 0; j < i; j++){	// 被るかチェック
				if(next[j] == n){
					flag = 0;
					break;
				}
			}
			if(flag == 0){			// 被ったらやり直し
				i--;
				flag = 1;
			}else{					// 被らなかったら次へ
				next[i] = n;
			}
		}
		counter = 0;				// カウンタリセット
	}
	
	current.x = 0;					// 出現位置
	current.y = 2;					// 最上段
	current.type = next[counter];	// 次のブロックを選択
	current.rotate = randN(4);		// 回転状態はランダム
	if(putBlock(current, 0) == 0){	// 置く。置けなかったら0を返す（ゲームオーバー用）
		return 0;
	}
	counter++;						// カウントアップ
	return 1;						// 置けたら1を返す
}

// ブロックを設置する関数
// actionが0ならチェック。1ならputを実行
// 外部から呼び出すときは必ずactionは0で呼び出す。
int putBlock(STATUS s, int action){
	int i, j;
	if(board[s.x][s.y] != 0){					// 置きたい位置にブロックがあったら失敗
		return 0;
	}
	
	if(action == 1){							// actionが1のときは（絶対置けるので）
		board[s.x % HORIZONTAL][s.y] = s.type;	// 中心ブロックを置きたい位置に置く
	}
	
	// 中心ブロックの周りのブロックを置く
	for (i = 0; i < 3; ++i){
		int dx = block[s.type].p[i].x;				// 中心ブロックからの相対位置
		int dy = block[s.type].p[i].y;				// 上に同じ
		int r = s.rotate % block[s.type].rotate;	// 各ブロックの回転可能数で剰余を取る
		for (j = 0; j < r; ++j){					// 回転計算（行列）
			int nx = dx, ny = dy;
			dx = ny; dy = -nx;
		}
		
		// 負数の剰余を防ぐ
		if(dx < 0){
			dx += HORIZONTAL;
		}
		
		// 置きたい位置にブロックがあったら失敗
		if(board[(s.x + dx) % HORIZONTAL][s.y + dy] != 0){
			return 0;
		}
		
		// actionが1なら（絶対置けるはずなので）置く
		if(action == 1){
			board[(s.x + dx) % HORIZONTAL][s.y + dy] = s.type;
		}
	}
	
	// 以上のチェックを通り抜けたら、必ず置けるので、自身をaction=1で呼ぶ
	if(action == 0){
		putBlock(s, 1);
	}
	
	// 成功
	return 1;
}

// ブロックを消す関数
int deleteBlock(STATUS s){
	int i, j;
	
	board[s.x][s.y] = 0;							// 中心ブロックを消す
	for (i = 0; i < 3; ++i){
		int dx = block[s.type].p[i].x;				// この辺はputBlock()と同じ
		int dy = block[s.type].p[i].y;
		int r = s.rotate % block[s.type].rotate;
		for (j = 0; j < r; ++j){
			int nx = dx, ny = dy;
			dx = ny; dy = -nx;
		}
		if(dx < 0){
			dx += HORIZONTAL;
		}
		board[(s.x + dx) % HORIZONTAL][s.y + dy] = 0;	// ブロックを消す
	}
	return 0;
}

// ゲームオーバー時に呼ばれる。
void gameOver(){
	int x, y;
	// 全てのブロックを赤くする
	for(y = 0; y < VERTICAL + 2; y++){
		for(x = 0; x < HORIZONTAL; x++){
			if(board[x][y] != 0){
				board[x][y] = 6;	// 赤いのはZブロック
			}
		}
	}
	game_over = 1;
	printf("SCORE : %d\n", score);	// 標準出力にスコア表示
	printf("LEVEL : %d\n", level);	// 標準出力にレベル表示
	glutPostRedisplay();
}

// 一列揃ったら消す関数
void deleteLine(){
	int x, y, i, j, count = 0;
	
	// 全ての段について
	for (y = VERTICAL; y >= 1; y--){
		int flag = 1;						// ブロックがあるフラグ
		for(x = 0; x < HORIZONTAL; x++){	// ぐるっと一周チェック
			if(board[x][y] == 0){
				flag = 0;					// ブロックが無ければフラグを折る
			}
		}
		if(flag == 1){
			count++;								// 消した行数を保持
			for (j = y; j > 1; j--){
				for(i = 0; i < HORIZONTAL; i++){
					board[i][j] = board[i][j - 1];	// 上の段を下す
				}
			}
			y++;									// 降ろした段をチェックするため
		}
	}
	for(i = 1; i < count; i++){
		levelUp();						// 消した本数に応じてレベルアップ
	}
	switch(count){						// 消した本数に応じてスコアアップ
		case 1: score += 40; break;
		case 2: score += 100; break;
		case 3: score += 300; break;
		case 4: score += 1200; break;
	}
}

// timer関数。ブロックを一段落下させる
// time_intervalの間隔で呼ばれる
void timer(int value) {
	deleteBlock(current);			// 現在のブロックを削除
	current.y++;					// 現在のブロックを下に移動
	if(putBlock(current, 0) == 0){	// 移動できなければ
		current.y--;				// 一段上げて（元に戻して）
		putBlock(current, 0);		// 置く（置けるはず）
		
		deleteLine();				// ライン消しを行う
		
		// 次のブロックを生成
		if(createBlock() == 0){		// 生成できなければ
			deleteBlock(current);	// 一旦生成する隙間を空けて
			putBlock(current, 0);	// 無理やり生成して
			gameOver();				// ゲームオーバー（全ブロックを赤くして操作を無効化）
			return;	// timerを呼ばない
		}else{
			if(level % 100 != 99 || level != 998){	// レベルアップ条件を満たせば
				levelUp();			// レベルアップ
			}
		}
	}
	glutPostRedisplay();
	glutTimerFunc(time_interval, timer, 0);			// time_interval(ms)後に再び呼び出す
}

// キーボード関数。回転で使用
void keyboard(unsigned char key, int x, int y){
	// ゲームオーバーなら入力を無視
	if(game_over == 1){
		return;
	}
	STATUS n = current;			// 現在の状態をコピー
	
	switch(key){
		case 'x':{
			n.rotate--;			// 右に回転
			// 負数の剰余を回避
			if(n.rotate < 0){
				n.rotate += block[n.type].rotate;
			}
			break;
		}
		case 'z':{				// 左に回転
			n.rotate++;
			break;
		}
	}
	deleteBlock(current);		// 現在のブロックを削除
	if(putBlock(n, 0) == 1){	// 新しい状態で置けるなら置く
		current = n;			// 現在の状態を更新
	}else{						// 回転できないなら
		putBlock(current, 0);	// 現在の状態で再び置く（一回削除してるので）
	}
	glutPostRedisplay();
}

// 特殊キー関数。矢印キーでのブロックの移動に使用。
void special(int key, int x, int y){
	// ゲームオーバーなら入力を無視
	if(game_over == 1){
		return;
	}
	
	STATUS n = current;			// 現在の状態をコピー
	int bonus = 0;				// 瞬間落下にはボーナスを用意
	
	switch(key){
		case GLUT_KEY_LEFT:{				// 左キー
			if(n.x == 0){					// x座標が0なら
				n.x = HORIZONTAL;			// 列数にする（負数の剰余を回避するため）
			}
			n.x = (n.x - 1) % HORIZONTAL;	// x座標の値を1減らして列数で剰余を取る
			break;
		}
		case GLUT_KEY_RIGHT:{				// 右キー
			n.x = (n.x + 1) % HORIZONTAL;	// x座標の値を1増やして列数で剰余を取る
			break;
		}
		case GLUT_KEY_DOWN:{				// y座標の値を1増やす（下方向）
			n.y++;
			break;
		}
		case GLUT_KEY_UP:{					// 上キー（強制落下）
			deleteBlock(current);			// 現在のブロックを削除
			while(putBlock(n, 0) == 1){		// 置ける間ずっと
				deleteBlock(n);				// ブロックを消して
				n.y++;						// 一つ下に移動して
				bonus++;					// ボーナスポイント加算
			}								// 置けなくなったら
			n.y--;							// ひとつ上に戻す
			bonus--;						// ボーナスポイント減算
			break;
		} 
	}
	
	deleteBlock(current);					// 現在のブロックを削除
	if(putBlock(n, 0) == 1){				// 新しい位置に置けたら
		score += bonus;						// ボーナスポイント加算
		current = n;						// 現在の状況を更新
	}else{									// 置けなかったら
		putBlock(current, 0);				// 現在のブロックを置く（元に戻す）
	}
	
	glutPostRedisplay();
}

// テトリス初期化関数
void Init(){
	int x, y;
	srand(time(NULL));							// 乱数シード
	for (x = 0; x < HORIZONTAL; ++x){
		for (y = 0; y < VERTICAL + 2; ++y){
			if(y == 0 || y == VERTICAL + 1){	// 最上段か最下段なら
				board[x][y] = 1;				// 壁を設置
			}else{								// 途中の段なら
				board[x][y] = 0;				// 空白にする（NULLブロック設置）
			}
		}
	}
	createBlock();
}

// ブロックを置く（直方体ではない（台形柱）なのでポリゴンで描画）
void cube(){
	float thick = (2 * RADIUS + 1) * tan(M_PI / HORIZONTAL);	// 台形の中央部の長さ
	glBegin(GL_QUADS);
		// 底
		glVertex3f(0.0, 0.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(0.0, 0.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 0.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 0.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		// 上
		glVertex3f(0.0, 1.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(0.0, 1.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(thick, 1.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(thick, 1.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
		// 内
		glVertex3f(0.0, 0.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(0.0, 1.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(0.0, 1.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(0.0, 0.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		// 外
		glVertex3f(thick, 0.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 1.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 1.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(thick, 0.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		// 手前
		glVertex3f(0.0, 1.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(0.0, 0.0, RADIUS * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(thick, 0.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		glVertex3f(thick, 1.0, (RADIUS + thick) * tan(M_PI / HORIZONTAL) - MARGIN);
		// 奥
		glVertex3f(0.0, 0.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(0.0, 1.0, -RADIUS * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 1.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
		glVertex3f(thick, 0.0, -(RADIUS + thick) * tan(M_PI / HORIZONTAL) + MARGIN);
	glEnd();
}

// テトリスフィールド表示関数
void showBoard(){
	int x, y;
	glPushMatrix();										// 行列退避
	glTranslatef(0.0, VERTICAL * (1 + MARGIN), 0.0);	// 上に移動
	
	for(y = 0; y < VERTICAL + 2; ++y){					// 壁を含む全段について
		glTranslatef(0.0, -(1.0 + MARGIN), 0.0);		// 下へ移動
		
		for(x = 0; x < HORIZONTAL; ++x){				// ぐるっと一周
			glRotatef(360.0 / HORIZONTAL, 0, 1, 0);		// 回転（デフォルトで30度）
			if(board[x][y] != 0){						// NULL以外なら
				glPushMatrix();							// 行列退避
					glMaterialfv(GL_FRONT, GL_AMBIENT, block_color[board[x][y]]);	// 色設定
					glTranslatef(RADIUS, 0.0, 0.0);		// x方向へRADIUSだけ移動
					cube();								// ブロックを描画
				glPopMatrix();							// 平行移動行列を削除
			}
		}
	}
	glPopMatrix();
}

// パズル実装ここまで-------------------------------------------------
// -------------------------------------------------------------------

// 整数を文字列に変換
void intToString(int n, char *str){
	int keta = 0, i, d = n;
	// 0だけ特別処理
	if(n == 0){
		str[0] = '0';
		str[1] = '\0';
		return;
	}
	// 桁数計算
	while(d != 0){
		d /= 10;
		keta++;
	}
	// 上の桁から文字に変換して配列に代入
	for(i = 0; i < keta; i++){
		str[keta - i - 1] = (char)('0' + n % 10);
		n /= 10;
	}
	// 終端文字
	str[keta] = '\0';
}

// 文字を指定位置に描画する関数
printString(int x, int y, char *str){
	int i;
	glColor3d(1.0, 1.0, 1.0);
	glRasterPos2d(x, WND_HEIGHT - y - 10);
	for(i = 0; str[i] != '\0'; i++){
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, str[i]);
	}
}

// テトリスのスコアとレベルを表示する関数
void print(){
	char str[51];
	printString(0, 0, "SCORE");
	intToString(score, str);
	printString(0, 15, str);
	printString(0, 35, "LEVEL");
	intToString(level, str);
	printString(0, 50, str);
}

// 文字を描画するための射影変換など。
// http://seesaawiki.jp/w/mikk_ni3_92/d/3D%A4%C82D%CA%B8%BB%FA%CE%F3 を参考
void DISPLAY_TEXT(){
	glPushAttrib(GL_ENABLE_BIT);						// 入れた方がいいらしい
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
			glLoadIdentity();
			gluOrtho2D(0, WND_WIDTH, 0, WND_HEIGHT);	// 左下を(0,0)とする正射影
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
				glLoadIdentity();						// モデルビュー変換を初期化
				print();								// 文字を描画
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	glPopAttrib();
	glMatrixMode(GL_MODELVIEW);
}

// 画面を描画する関数
void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	// 視点の設定
	gluLookAt(	eye.x, eye.y, eye.z,
				lookat.x, lookat.y, lookat.z,
				up.x, up.y, up.z);
	
	// 盤面の表示
	glPushMatrix();
		showBoard();
	glPopMatrix();
	
	// 平面に文字を描画するため、デプステストを無効化
	glDisable(GL_DEPTH_TEST);
	
	// 文字を描画
	DISPLAY_TEXT();
	
	// 表示
	glutSwapBuffers();
	glFlush();
}

// 画面のリサイズ
void resize(int w, int h)
{
	glViewport(0, 0, w, h);
	
	// 透視変換行列の指定
	glMatrixMode(GL_PROJECTION);
	
	// 透視変換行列の初期化
	glLoadIdentity();
	gluPerspective(40.0, (double)w / (double)h, 1.0, 300.0);
}

// マウス関数。直線の描画や修正を行う
void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON){
		if(state == GLUT_DOWN){
			motion_prev.x = x;									// 直前のカーソル位置を保存
			motion_prev.y = y;									// 上に同じ
			if(glutGetModifiers() == GLUT_ACTIVE_CTRL){			// CTRLキーが押されていれば
				ctrl = 1;										// CTRLフラグを立てる
			}else if(glutGetModifiers() == GLUT_ACTIVE_SHIFT){	// SHIFTキーが押されていれば
				shift = 1;										// SHIFTフラグを立てる
			}
		}else{
			ctrl = shift = 0;
		}
	}else if(button == GLUT_RIGHT_BUTTON){						// 右ボタンが
		if(state == GLUT_DOWN){									// 押下されたら
			right_click = 1;									// 右クリックフラグを立てる
			motion_prev.x = x;
			motion_prev.y = y;
		}else if(state == GLUT_UP){								// 離されたら
			right_click = 0;									// 右クリックフラグを折る
		}
	}
}

// 3次元実数ベクトルの大きさを計算
float length(vec3f vec){
	return (float)sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

// 3次元実数ベクトルを正規化（長さを1にする）
void normalize(vec3f *vec){
	float len = length(*vec);
	vec->x /= len;
	vec->y /= len;
	vec->z /= len;
}

// 3次元実数ベクトルの内積を計算
float dot(vec3f v1, vec3f v2){
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

// 3次元実数ベクトルの外積を計算
vec3f cross(vec3f v1, vec3f v2){
	vec3f ans;
	ans.x = v1.y * v2.z - v1.z * v2.y;
	ans.y = v1.z * v2.x - v1.x * v2.z;
	ans.z = v1.x * v2.y - v1.y * v2.x;
	return ans;
}

// 2次元整数ベクトルの外積を計算
int cross2(vec2i v1, vec2i v2){
	return (v1.x * v2.y - v1.y * v2.x);
}

// 3次元実数ベクトルの引き算
vec3f sub(vec3f v1, vec3f v2){
	vec3f ans;
	ans.x = v1.x - v2.x;
	ans.y = v1.y - v2.y;
	ans.z = v1.z - v2.z;
	return ans;
}

// 3次元実数ベクトルの足し算
vec3f add(vec3f v1, vec3f v2){
	vec3f ans;
	ans.x = v1.x + v2.x;
	ans.y = v1.y + v2.y;
	ans.z = v1.z + v2.z;
	return ans;
}

// 3次元実数ベクトルの実数倍
vec3f extent(float a, vec3f vec){
	vec3f ans;
	ans.x = vec.x * a;
	ans.y = vec.y * a;
	ans.z = vec.z * a;
	return ans;
}

// モーション関数。視点の変更を行う
void motion(int x , int y){
	vec2i motion_diff;								// カーソルの変位ベクトル
	motion_diff.x = x - motion_prev.x;
	motion_diff.y = y - motion_prev.y;
	
	if(right_click == 1){							// 右ドラッグなら（平面回転）
		vec2i prev;									// 前回位置の、画面中心から見た位置
		prev.x = motion_prev.x - WND_WIDTH / 2;
		prev.y = motion_prev.y - WND_HEIGHT / 2;
		
		int rot = cross2(prev, motion_diff);		// 前回位置と変位の外積（回転量）
		
		vec3f disp_x;								// 画面横方向単位ベクトルの空間内での表現（実態）
		
		disp_x = cross(look, up);					// 視線と画面上方向の外積で得られる
		normalize(&disp_x);
		
		up = add(up, extent(-0.00003 * rot, disp_x));	// 上方向ベクトルを変更
		normalize(&up);								// 正規化
		
		motion_prev.x = x;							// 前回位置を更新
		motion_prev.y = y;							// 上に同じ
	}else{											// 左ドラッグなら
		if(ctrl == 1){								// CTRLが押されていれば（ズーム）
			eye = add(eye, extent(-0.08 * motion_diff.y, look));	// 視点位置を視線方向に移動
			look_dist = length(sub(lookat, eye));	// 視点・注視点の距離を更新
			
			motion_prev.x = x;
			motion_prev.y = y;
		}else if(shift == 1){						// SHIFTが押されていれば（平行移動）
			vec3f disp_x;							// 画面右方向単位ベクトル
			vec3f disp_y;							// 画面上方向単位ベクトル
			
			disp_x = cross(look, up);
			normalize(&disp_x);
			disp_y = up;							// 画面上方向はupベクトルそのまま利用
			// disp_y = sub(up, extent(dot(up, look), look));	// 上方向ベクトルを視線方向ベクトルに直交化
			normalize(&disp_y);
			
			// 視点と注視点の移動方向と大きさ
			vec3f eye_move = add(extent((float)-motion_diff.x, disp_x), extent((float)motion_diff.y, disp_y));
			eye_move = extent(0.05, eye_move);		// 0.05は体感でちょうどいい補正値
			
			eye = add(eye, eye_move);				// 視点を更新
			
			lookat = add(lookat, eye_move);			// 注視点を更新
			
			motion_prev.x = x;
			motion_prev.y = y;
		}else{										// 特殊キーが押されていなければ（対象の3D回転）
			vec3f disp_x;
			vec3f disp_y;
			
			disp_x = cross(look, up);
			normalize(&disp_x);
			disp_y = up;
			// disp_y = sub(up, extent(dot(up, look), look));
			normalize(&disp_y);
			
			vec3f eye_move = add(extent((float)-motion_diff.x, disp_x), extent((float)motion_diff.y, disp_y));
			eye_move = extent(0.015 * look_dist, eye_move);
			
			eye = add(eye, eye_move);				// 視点のみ移動
			
			
			look = sub(lookat, eye);				// 視線方向を更新
			normalize(&look);
			
			eye = add(eye, extent(length(sub(lookat, eye)) - look_dist, look));	// 視点の距離補正
			
			up = sub(up, extent(dot(up, look), look));	// 上方向を更新（直交化法を利用）
			normalize(&up);
			
			motion_prev.x = x;
			motion_prev.y = y;
		}
	}
	glutPostRedisplay();
}

// main関数
int main(int argc, char *argv[]){
	/*
	if(argc == 2 && strcmp(argv[0], "-help")){
		help();
		return;
	}*/
	glutInitWindowSize(WND_WIDTH, WND_HEIGHT);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("CycloTETRiS");
	glutDisplayFunc(display);
	glutMotionFunc(motion);
	glutReshapeFunc(resize);
	glutMouseFunc(mouse);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutTimerFunc(time_interval, timer, 0);
	glClearColor(0, 0, 0, 0);
	
	glLightfv(GL_LIGHT0 , GL_POSITION , lightPos);	// 光源の位置を設定
	glLightfv(GL_LIGHT0 , GL_AMBIENT , lightCol);	// 光の色と種類を設定
	glEnable(GL_LIGHTING);							// 光を有効化
	glEnable(GL_LIGHT0);							// 光0を有効化
	glEnable(GL_CULL_FACE);							// 描画しない面を設定
	glCullFace(GL_BACK);							// 裏面を描画しない（動作の軽量化のため）
	glEnable(GL_NORMALIZE);							// 法線ベクトルの正規化を有効化
	Init();											// パズルを初期化
	
	glutMainLoop();
	return 0; 
}