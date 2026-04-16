/*
 * CALC 24 GAME — Graphical Edition (raylib)
 *
 * Click the 4 cards to insert numbers, use the operator buttons
 * to build an expression, then hit CHECK to see if it equals 24.
 * Use CUSTOM to enter your own 4 numbers.
 *
 * Build:
 *   gcc -O2 -std=c99 calc24_gui.c -o calc24_gui.exe \
 *       -I/c/msys64/mingw64/include \
 *       -L/c/msys64/mingw64/lib -lraylib -lopengl32 -lgdi32 -lwinmm
 */

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* ── Layout constants ─────────────────────────────────────────────────── */
#define SCREEN_W   900
#define SCREEN_H   640
#define TARGET     24
#define EPS        1e-9
#define INF        1e18

/* ── Colours ──────────────────────────────────────────────────────────── */
#define COL_BG      CLITERAL(Color){ 26,  26,  46, 255}
#define COL_PANEL   CLITERAL(Color){ 15,  52,  96, 255}
#define COL_PANEL2  CLITERAL(Color){ 20,  30,  60, 255}
#define COL_BORDER  CLITERAL(Color){ 22,  33,  62, 255}
#define COL_GOLD    CLITERAL(Color){245, 197,  24, 255}
#define COL_GREEN   CLITERAL(Color){ 46, 204, 113, 255}
#define COL_RED     CLITERAL(Color){231,  76,  60, 255}
#define COL_PURPLE  CLITERAL(Color){155,  89, 182, 255}
#define COL_CYAN    CLITERAL(Color){ 52, 152, 219, 255}
#define COL_GRAY    CLITERAL(Color){ 85,  85,  85, 255}
#define COL_WHITE   WHITE
#define COL_DARK    CLITERAL(Color){ 26,  26,  46, 255}
#define COL_CARD    CLITERAL(Color){240, 240, 235, 255}
#define COL_OVERLAY CLITERAL(Color){  0,   0,   0, 160}

/* ═══════════════════════════════════════════════════════════════════════
   SOLVER
   ═══════════════════════════════════════════════════════════════════════ */
static const char *OP_SYM[] = {"+","-","*","/"};

static double apply_op(double a, double b, int op) {
    switch (op) {
        case 0: return a + b;
        case 1: return a - b;
        case 2: return a * b;
        case 3: return fabs(b) < EPS ? INF : a / b;
    }
    return INF;
}

static double eval_struct(double *n, int *o, int s) {
    double a=n[0],b=n[1],c=n[2],d=n[3];
    switch (s) {
        case 0: return apply_op(apply_op(apply_op(a,b,o[0]),c,o[1]),d,o[2]);
        case 1: return apply_op(apply_op(a,apply_op(b,c,o[0]),o[1]),d,o[2]);
        case 2: return apply_op(apply_op(a,b,o[0]),apply_op(c,d,o[1]),o[2]);
        case 3: return apply_op(a,apply_op(apply_op(b,c,o[0]),d,o[1]),o[2]);
        case 4: return apply_op(a,apply_op(b,apply_op(c,d,o[0]),o[1]),o[2]);
    }
    return INF;
}

static void fmt_struct(double *n, int *o, int s, char *buf, int sz) {
    char a[16],b[16],c[16],d[16];
    snprintf(a,16,"%.4g",n[0]); snprintf(b,16,"%.4g",n[1]);
    snprintf(c,16,"%.4g",n[2]); snprintf(d,16,"%.4g",n[3]);
    switch (s) {
        case 0: snprintf(buf,sz,"((%s%s%s)%s%s)%s%s",a,OP_SYM[o[0]],b,OP_SYM[o[1]],c,OP_SYM[o[2]],d); break;
        case 1: snprintf(buf,sz,"(%s%s(%s%s%s))%s%s",a,OP_SYM[o[1]],b,OP_SYM[o[0]],c,OP_SYM[o[2]],d); break;
        case 2: snprintf(buf,sz,"(%s%s%s)%s(%s%s%s)",a,OP_SYM[o[0]],b,OP_SYM[o[2]],c,OP_SYM[o[1]],d); break;
        case 3: snprintf(buf,sz,"%s%s((%s%s%s)%s%s)",a,OP_SYM[o[2]],b,OP_SYM[o[0]],c,OP_SYM[o[1]],d); break;
        case 4: snprintf(buf,sz,"%s%s(%s%s(%s%s%s))",a,OP_SYM[o[2]],b,OP_SYM[o[1]],c,OP_SYM[o[0]],d); break;
    }
}

static double perms[24][4];
static int    perm_cnt;
static void swapd(double *a, double *b) { double t=*a;*a=*b;*b=t; }
static void gen_perms(double *arr, int start, int n) {
    if (start==n){ memcpy(perms[perm_cnt++],arr,n*sizeof(double)); return; }
    for (int i=start;i<n;i++){
        swapd(&arr[start],&arr[i]); gen_perms(arr,start+1,n);
        swapd(&arr[start],&arr[i]);
    }
}
static int solve(int *nums, char *out, int outsz) {
    double arr[4]={(double)nums[0],(double)nums[1],(double)nums[2],(double)nums[3]};
    perm_cnt=0; gen_perms(arr,0,4);
    for (int p=0;p<24;p++)
    for (int o1=0;o1<4;o1++) for (int o2=0;o2<4;o2++) for (int o3=0;o3<4;o3++) {
        int ops[3]={o1,o2,o3};
        for (int s=0;s<5;s++) {
            double v=eval_struct(perms[p],ops,s);
            if (fabs(v-TARGET)<EPS){ if(out) fmt_struct(perms[p],ops,s,out,outsz); return 1; }
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
   EXPRESSION PARSER / EVALUATOR
   ═══════════════════════════════════════════════════════════════════════ */
static const char *pp;
static int pnums[8], pnum_cnt, perr;

static double pe_expr(void);
static double pe_term(void);
static double pe_factor(void);

static void pe_skip(void){ while(*pp==' '||*pp=='\t') pp++; }
static double pe_factor(void){
    pe_skip();
    if(*pp=='('){ pp++; double v=pe_expr(); pe_skip(); if(*pp==')') pp++; else perr=1; return v; }
    if(*pp>='0'&&*pp<='9'){
        int n=0; while(*pp>='0'&&*pp<='9'){n=n*10+(*pp-'0');pp++;}
        if(pnum_cnt<8) pnums[pnum_cnt++]=n; return (double)n;
    }
    perr=1; return 0;
}
static double pe_term(void){
    double v=pe_factor();
    for(;;){ pe_skip();
        if(*pp=='*'){pp++;v*=pe_factor();}
        else if(*pp=='/'){pp++;double d=pe_factor();if(fabs(d)<EPS)perr=1;else v/=d;}
        else break;
    } return v;
}
static double pe_expr(void){
    double v=pe_term();
    for(;;){ pe_skip();
        if(*pp=='+'){pp++;v+=pe_term();}
        else if(*pp=='-'){pp++;v-=pe_term();}
        else break;
    } return v;
}
static double evaluate(const char *s, int *used, int *ucnt){
    pp=s; pnum_cnt=0; perr=0;
    double r=pe_expr(); pe_skip();
    if(*pp!='\0') perr=1;
    if(used) memcpy(used,pnums,pnum_cnt*sizeof(int));
    if(ucnt) *ucnt=pnum_cnt;
    return perr?INF:r;
}
static int nums_match(int *used, int cnt, int *cards){
    if(cnt!=4) return 0;
    int ca[4],ua[4];
    memcpy(ca,cards,4*sizeof(int)); memcpy(ua,used,4*sizeof(int));
    for(int i=1;i<4;i++){
        int t,j;
        t=ca[i];for(j=i;j>0&&ca[j-1]>t;j--)ca[j]=ca[j-1];ca[j]=t;
        t=ua[i];for(j=i;j>0&&ua[j-1]>t;j--)ua[j]=ua[j-1];ua[j]=t;
    }
    for(int i=0;i<4;i++) if(ca[i]!=ua[i]) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════
   CARD DISPLAY LABEL
   Works for 1-13 (standard deck faces) and any other positive integer.
   ═══════════════════════════════════════════════════════════════════════ */
static const char *face(int n){
    static char buf[8];
    static const char *tbl[]={"","A","2","3","4","5","6","7","8","9","10","J","Q","K"};
    if(n>=1&&n<=13) return tbl[n];
    snprintf(buf,sizeof(buf),"%d",n);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
   BUTTON HELPER
   ═══════════════════════════════════════════════════════════════════════ */
typedef struct { Rectangle rect; const char *label; Color bg; Color fg; } Btn;

static void draw_btn(Btn b, int hovered){
    Color bg = hovered ? Fade(b.bg,0.70f) : b.bg;
    DrawRectangleRounded(b.rect,0.25f,8,bg);
    DrawRectangleRoundedLinesEx(b.rect,0.25f,8,1.5f,Fade(WHITE,0.15f));
    int fw=MeasureText(b.label,20);
    DrawText(b.label,(int)(b.rect.x+(b.rect.width-fw)/2),
             (int)(b.rect.y+(b.rect.height-20)/2),20,b.fg);
}
static int btn_clicked(Btn b){
    return CheckCollisionPointRec(GetMousePosition(),b.rect)
        && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}
static int btn_hovered(Btn b){
    return CheckCollisionPointRec(GetMousePosition(),b.rect);
}

/* ═══════════════════════════════════════════════════════════════════════
   TOKEN / EXPRESSION
   ═══════════════════════════════════════════════════════════════════════ */
#define MAX_TOKENS 32
typedef struct { char text[8]; int card_idx; } Token;

static void build_expr_str(Token *tokens, int tok_cnt, char *out, int outsz){
    out[0]='\0';
    for(int i=0;i<tok_cnt;i++){
        if(i) strncat(out," ",outsz-strlen(out)-1);
        strncat(out,tokens[i].text,outsz-strlen(out)-1);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   GAME STATE
   ═══════════════════════════════════════════════════════════════════════ */
typedef enum { MSG_NONE, MSG_OK, MSG_ERR, MSG_INFO } MsgKind;

static int    cards[4];
static int    used[4];
static Token  tokens[MAX_TOKENS];
static int    tok_cnt;
static char   msg_text[128];
static MsgKind msg_kind;
static float  msg_timer;
static int    solved, skipped;
static char   hint_text[160];
static int    show_hint;
static float  flash;
static float  anim;

/* Custom-number modal state */
static int    custom_mode   = 0;       /* modal open? */
static char   custom_buf[4][8];        /* text typed into each of the 4 fields */
static int    custom_focused = 0;      /* which field has keyboard focus */
static char   custom_err[80];          /* validation error message */

/* ── Helpers ── */
static void new_round(void){
    for(int i=0;i<4;i++) cards[i]=(rand()%13)+1;
    memset(used,0,sizeof(used));
    tok_cnt=0; msg_text[0]='\0'; msg_kind=MSG_NONE; msg_timer=0;
    show_hint=0; hint_text[0]='\0';
}
static void push_token(const char *txt, int card_idx){
    if(tok_cnt>=MAX_TOKENS) return;
    strncpy(tokens[tok_cnt].text,txt,7);
    tokens[tok_cnt].card_idx=card_idx;
    tok_cnt++;
}
static void pop_token(void){
    if(!tok_cnt) return;
    tok_cnt--;
    int ci=tokens[tok_cnt].card_idx;
    if(ci>=0) used[ci]=0;
}
static void clear_expr(void){ tok_cnt=0; memset(used,0,sizeof(used)); }
static void set_msg(const char *txt, MsgKind kind){
    strncpy(msg_text,txt,sizeof(msg_text)-1);
    msg_kind=kind; msg_timer=3.0f;
}
static void check_answer(void){
    char expr[256]; build_expr_str(tokens,tok_cnt,expr,sizeof(expr));
    if(!tok_cnt){ set_msg("Build an expression first!",MSG_ERR); return; }
    int u[8],ucnt=0;
    double r=evaluate(expr,u,&ucnt);
    if(perr||r>INF/2){ set_msg("Invalid expression!",MSG_ERR); return; }
    if(!nums_match(u,ucnt,cards)){ set_msg("Use all 4 numbers exactly once!",MSG_ERR); return; }
    if(fabs(r-TARGET)<EPS){
        set_msg("CORRECT!  = 24",MSG_OK);
        solved++; flash=1.2f; show_hint=0;
    } else {
        char tmp[80]; snprintf(tmp,sizeof(tmp),"= %.5g  — not 24. Try again!",r);
        set_msg(tmp,MSG_ERR);
    }
}

/* Open the custom-number modal, pre-filling fields with current cards */
static void open_custom_modal(void){
    for(int i=0;i<4;i++)
        snprintf(custom_buf[i],sizeof(custom_buf[i]),"%d",cards[i]);
    custom_focused=0; custom_err[0]='\0'; custom_mode=1;
}

/* Confirm the custom modal — parse & validate, then apply */
static void confirm_custom(void){
    int vals[4];
    for(int i=0;i<4;i++){
        if(custom_buf[i][0]=='\0'){ snprintf(custom_err,sizeof(custom_err),"Card %d is empty.",i+1); return; }
        int v=atoi(custom_buf[i]);
        if(v<1||v>999){ snprintf(custom_err,sizeof(custom_err),"Card %d: enter a number between 1 and 999.",i+1); return; }
        vals[i]=v;
    }
    for(int i=0;i<4;i++) cards[i]=vals[i];
    memset(used,0,sizeof(used));
    tok_cnt=0; msg_text[0]='\0'; msg_kind=MSG_NONE; msg_timer=0;
    show_hint=0; hint_text[0]='\0';
    custom_mode=0;
}

/* ═══════════════════════════════════════════════════════════════════════
   DRAW CUSTOM MODAL
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_custom_modal(int cx, int cy, float anim_t){
    /* Dim overlay */
    DrawRectangle(0,0,SCREEN_W,SCREEN_H,COL_OVERLAY);

    /* Panel */
    int pw=520, ph=300;
    int px=cx-pw/2, py=cy-ph/2;
    Rectangle panel={(float)px,(float)py,(float)pw,(float)ph};
    DrawRectangleRounded(panel,0.08f,10,COL_PANEL2);
    DrawRectangleRoundedLinesEx(panel,0.08f,10,2,COL_CYAN);

    /* Title */
    const char *title="Custom Numbers";
    DrawText(title,px+(pw-MeasureText(title,26))/2,py+18,26,COL_GOLD);

    /* Subtitle */
    const char *sub="Type any 4 numbers (1 – 999), then press OK";
    DrawText(sub,px+(pw-MeasureText(sub,14))/2,py+52,14,(Color){170,170,200,255});

    /* 4 input fields */
    int fw=90, fh=60, fgap=18;
    int fields_w=4*fw+3*fgap;
    int fx0=cx-fields_w/2;
    int fy=py+88;

    for(int i=0;i<4;i++){
        int fx=fx0+i*(fw+fgap);
        Rectangle fr={(float)fx,(float)fy,(float)fw,(float)fh};

        /* Field background + border */
        Color border = (i==custom_focused) ? COL_CYAN : COL_BORDER;
        Color bg     = (i==custom_focused) ? (Color){20,60,110,255} : COL_PANEL;
        DrawRectangleRounded(fr,0.15f,8,bg);
        DrawRectangleRoundedLinesEx(fr,0.15f,8,2,border);

        /* Label above */
        char lbl[8]; snprintf(lbl,sizeof(lbl),"Card %d",i+1);
        DrawText(lbl,fx+(fw-MeasureText(lbl,13))/2,fy-20,13,(Color){160,160,190,255});

        /* Typed value + blinking cursor on focused field */
        char disp[12];
        if(i==custom_focused)
            snprintf(disp,sizeof(disp),"%s%s",custom_buf[i],((int)(anim_t*2)%2)?"|":"");
        else
            snprintf(disp,sizeof(disp),"%s",custom_buf[i][0]?custom_buf[i]:"_");

        int tw=MeasureText(disp,24);
        DrawText(disp,fx+(fw-tw)/2,fy+(fh-24)/2,24,
                 (i==custom_focused)?COL_GOLD:COL_WHITE);
    }

    /* Tab hint */
    DrawText("Tab / click to switch field",
             cx-MeasureText("Tab / click to switch field",13)/2,
             fy+fh+10,13,(Color){100,100,130,255});

    /* Error */
    if(custom_err[0]){
        int ew=MeasureText(custom_err,15);
        DrawText(custom_err,cx-ew/2,fy+fh+32,15,COL_RED);
    }

    /* OK / Cancel buttons */
    int bw=110, bh=42, bgy=py+ph-60;
    Btn ok_btn    = {{(float)(cx-bw-10),(float)bgy,(float)bw,(float)bh},"OK",   COL_GREEN, COL_DARK};
    Btn cancel_btn= {{(float)(cx+10),   (float)bgy,(float)bw,(float)bh},"Cancel",COL_GRAY, COL_WHITE};
    draw_btn(ok_btn,    btn_hovered(ok_btn));
    draw_btn(cancel_btn,btn_hovered(cancel_btn));

    /* Clicks on OK / Cancel */
    if(btn_clicked(ok_btn))     confirm_custom();
    if(btn_clicked(cancel_btn)) { custom_mode=0; }

    /* Clicks directly on field boxes to focus them */
    for(int i=0;i<4;i++){
        int fx=fx0+i*(fw+fgap);
        Rectangle fr={(float)fx,(float)fy,(float)fw,(float)fh};
        if(CheckCollisionPointRec(GetMousePosition(),fr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            custom_focused=i;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════ */
int main(void){
    srand((unsigned)time(NULL));

    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W,SCREEN_H,"Calc 24 Game");
    SetTargetFPS(60);
    new_round();

    while(!WindowShouldClose()){
        float dt=GetFrameTime();
        anim+=dt;
        if(msg_timer>0) msg_timer-=dt;
        if(flash>0){ flash-=dt; if(flash<=0){flash=0;new_round();} }

        /* ── Layout values (computed once per frame) ── */
        int cx=SCREEN_W/2, cy=SCREEN_H/2;

        int card_w=120, card_h=170, card_gap=18;
        int cards_total=4*card_w+3*card_gap;
        int cards_x0=cx-cards_total/2;
        int cards_y=80;

        int op_y=cards_y+card_h+28;
        int op_w=66, op_h=52, op_gap=10;
        const char *op_labels[]=  {"+","-","*","/","(",")"};
        int n_ops=6;
        int op_row_w=n_ops*op_w+(n_ops-1)*op_gap;
        int op_x0=cx-op_row_w/2;

        int expr_y=op_y+op_h+16;
        int expr_h=52;
        Rectangle expr_rect={(float)(cx-320),(float)expr_y,640,(float)expr_h};

        /* 6 action buttons: BACK CLEAR CHECK HINT NEW CUSTOM */
        int act_y=expr_y+expr_h+16;
        int act_w=100, act_h=46, act_gap=10;
        const char *act_labels[]={"BACK","CLEAR","CHECK","HINT","NEW","CUSTOM"};
        Color act_colors[]={ COL_GRAY, COL_RED, COL_GOLD, COL_PURPLE, COL_GREEN, COL_CYAN };
        Color act_fg[]=    { COL_WHITE,COL_WHITE,COL_DARK,COL_WHITE,  COL_DARK,  COL_DARK };
        int n_acts=6;
        int act_row_w=n_acts*act_w+(n_acts-1)*act_gap;
        int act_x0=cx-act_row_w/2;

        int msg_y=act_y+act_h+20;
        int hint_y=msg_y+34;

        /* ── Input: custom modal steals all keyboard input ── */
        if(custom_mode){
            /* Keyboard for the focused field */
            int key=GetCharPressed();
            while(key>0){
                char ch=(char)key;
                if(ch>='0'&&ch<='9'){
                    int len=(int)strlen(custom_buf[custom_focused]);
                    if(len<3){ custom_buf[custom_focused][len]=ch; custom_buf[custom_focused][len+1]='\0'; }
                    custom_err[0]='\0';
                }
                key=GetCharPressed();
            }
            if(IsKeyPressed(KEY_BACKSPACE)){
                int len=(int)strlen(custom_buf[custom_focused]);
                if(len>0) custom_buf[custom_focused][len-1]='\0';
                custom_err[0]='\0';
            }
            if(IsKeyPressed(KEY_TAB))    custom_focused=(custom_focused+1)%4;
            if(IsKeyPressed(KEY_ENTER))  confirm_custom();
            if(IsKeyPressed(KEY_ESCAPE)) custom_mode=0;
        }
        else if(flash<=0){
            /* Normal game keyboard */
            int key=GetCharPressed();
            while(key>0){
                char ch=(char)key;
                if(ch=='+'||ch=='-'||ch=='*'||ch=='/'||ch=='('||ch==')'){
                    char s[3]={ch,0}; push_token(s,-1);
                }
                key=GetCharPressed();
            }
            if(IsKeyPressed(KEY_BACKSPACE)) pop_token();
            if(IsKeyPressed(KEY_ENTER))     check_answer();
            if(IsKeyPressed(KEY_ESCAPE))    clear_expr();

            /* Card clicks */
            for(int i=0;i<4;i++){
                Rectangle cr={(float)(cards_x0+i*(card_w+card_gap)),(float)cards_y,(float)card_w,(float)card_h};
                if(!used[i]&&CheckCollisionPointRec(GetMousePosition(),cr)&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    used[i]=1; push_token(face(cards[i]),i);
                    msg_text[0]='\0'; show_hint=0;
                }
            }

            /* Operator buttons */
            for(int i=0;i<n_ops;i++){
                Btn b={{(float)(op_x0+i*(op_w+op_gap)),(float)op_y,(float)op_w,(float)op_h},
                       op_labels[i],COL_PANEL,COL_GOLD};
                if(btn_clicked(b)){ push_token(op_labels[i],-1); msg_text[0]='\0'; show_hint=0; }
            }

            /* Action buttons */
            for(int i=0;i<n_acts;i++){
                Btn b={{(float)(act_x0+i*(act_w+act_gap)),(float)act_y,(float)act_w,(float)act_h},
                       act_labels[i],act_colors[i],act_fg[i]};
                if(btn_clicked(b)){
                    if(i==0){ pop_token(); msg_text[0]='\0'; }
                    if(i==1){ clear_expr(); msg_text[0]='\0'; show_hint=0; }
                    if(i==2){ check_answer(); }
                    if(i==3){
                        char sol[160];
                        if(solve(cards,sol,sizeof(sol))){
                            snprintf(hint_text,sizeof(hint_text),"Hint: %s = 24",sol);
                        } else {
                            snprintf(hint_text,sizeof(hint_text),"No solution — new cards!");
                            skipped++; flash=0.01f;
                        }
                        show_hint=1;
                    }
                    if(i==4){ skipped++; new_round(); }
                    if(i==5){ open_custom_modal(); }
                }
            }
        }

        /* ══════════════════════════════════════════════════
           DRAW
           ══════════════════════════════════════════════════ */
        BeginDrawing();
        ClearBackground(COL_BG);

        /* Title */
        const char *title="CALC  24";
        DrawText(title,cx-MeasureText(title,42)/2,16,42,COL_GOLD);

        /* Score */
        char score_buf[64];
        snprintf(score_buf,sizeof(score_buf),"Solved: %d     Skipped: %d",solved,skipped);
        DrawText(score_buf,cx-MeasureText(score_buf,18)/2,64,18,(Color){170,170,170,255});

        /* ── Cards ── */
        for(int i=0;i<4;i++){
            int cxi=cards_x0+i*(card_w+card_gap);
            Rectangle cr={(float)cxi,(float)cards_y,(float)card_w,(float)card_h};

            Color card_bg=COL_CARD;
            if(flash>0){
                float t=flash>0.6f?1.0f:flash/0.6f;
                card_bg=ColorLerp(COL_CARD,(Color){180,255,190,255},t);
            }

            float lift=0;
            if(!used[i]&&flash<=0&&!custom_mode&&CheckCollisionPointRec(GetMousePosition(),cr)) lift=6;
            Rectangle drawn=cr; drawn.y-=lift;

            if(used[i]){
                DrawRectangleRounded(drawn,0.12f,10,(Color){55,55,70,255});
                DrawRectangleRoundedLinesEx(drawn,0.12f,10,2,(Color){75,75,90,255});
            } else {
                DrawRectangleRounded((Rectangle){drawn.x+4,drawn.y+7,drawn.width,drawn.height},
                                     0.12f,10,(Color){0,0,0,80});
                DrawRectangleRounded(drawn,0.12f,10,card_bg);
                DrawRectangleRoundedLinesEx(drawn,0.12f,10,2,(Color){175,175,165,255});

                const char *lbl=face(cards[i]);
                int fs_big=(strlen(lbl)>=3)?36:(strlen(lbl)==2?44:52);
                int fs_sm=17;
                int bw=MeasureText(lbl,fs_big);

                Color ink=(Color){30,30,50,255};
                DrawText(lbl,(int)drawn.x+9,(int)drawn.y+7,fs_sm,ink);
                DrawText(lbl,(int)(drawn.x+(drawn.width-bw)/2),(int)(drawn.y+(drawn.height-fs_big)/2),fs_big,ink);
                int sw2=MeasureText(lbl,fs_sm);
                DrawText(lbl,(int)(drawn.x+drawn.width-sw2-9),(int)(drawn.y+drawn.height-fs_sm-7),fs_sm,ink);

                char badge[6]; snprintf(badge,sizeof(badge),"[%d]",i+1);
                DrawText(badge,(int)(drawn.x+(drawn.width-MeasureText(badge,12))/2),
                         (int)(drawn.y+drawn.height-20),12,(Color){120,115,105,255});
            }
        }

        /* ── Operator buttons ── */
        for(int i=0;i<n_ops;i++){
            Btn b={{(float)(op_x0+i*(op_w+op_gap)),(float)op_y,(float)op_w,(float)op_h},
                   op_labels[i],COL_PANEL,COL_GOLD};
            draw_btn(b,!custom_mode&&btn_hovered(b));
        }

        /* ── Expression display ── */
        char expr_str[256]; build_expr_str(tokens,tok_cnt,expr_str,sizeof(expr_str));
        Color expr_border=COL_BORDER, expr_text=COL_GOLD;
        if(msg_kind==MSG_OK  &&msg_timer>0){expr_border=COL_GREEN;expr_text=COL_GREEN;}
        if(msg_kind==MSG_ERR &&msg_timer>0){expr_border=COL_RED;  expr_text=COL_RED;  }

        DrawRectangleRounded(expr_rect,0.12f,8,COL_PANEL);
        DrawRectangleRoundedLinesEx(expr_rect,0.12f,8,2,expr_border);

        char disp_buf[264];
        if(tok_cnt) snprintf(disp_buf,sizeof(disp_buf),"%s%s",expr_str,(!custom_mode&&(int)(anim*2)%2)?" |":"");
        else        snprintf(disp_buf,sizeof(disp_buf),"_");
        int dw=MeasureText(disp_buf,22);
        if(dw>(int)expr_rect.width-20){
            BeginScissorMode((int)expr_rect.x+8,(int)expr_rect.y,(int)expr_rect.width-16,(int)expr_rect.height);
            DrawText(disp_buf,(int)(expr_rect.x+expr_rect.width-10-dw),(int)(expr_rect.y+(expr_rect.height-22)/2),22,expr_text);
            EndScissorMode();
        } else {
            DrawText(disp_buf,(int)(expr_rect.x+10),(int)(expr_rect.y+(expr_rect.height-22)/2),22,expr_text);
        }

        /* ── Action buttons ── */
        for(int i=0;i<n_acts;i++){
            Btn b={{(float)(act_x0+i*(act_w+act_gap)),(float)act_y,(float)act_w,(float)act_h},
                   act_labels[i],act_colors[i],act_fg[i]};
            draw_btn(b,!custom_mode&&btn_hovered(b));
        }

        /* ── Message ── */
        if(msg_timer>0&&msg_text[0]){
            Color mc=msg_kind==MSG_OK?COL_GREEN:msg_kind==MSG_ERR?COL_RED:COL_GOLD;
            DrawText(msg_text,cx-MeasureText(msg_text,22)/2,msg_y,22,mc);
        }

        /* ── Hint ── */
        if(show_hint&&hint_text[0]){
            int hw=MeasureText(hint_text,18);
            int hx=cx-hw/2;
            DrawRectangleRounded((Rectangle){(float)(hx-12),(float)(hint_y-8),(float)(hw+24),34},
                                 0.3f,6,COL_PANEL);
            DrawText(hint_text,hx,hint_y,18,(Color){200,200,200,255});
        }

        /* ── Footer ── */
        const char *footer="Click cards or type  |  +  -  *  /  (  )  |  Enter=Check  Esc=Clear  Backspace=Undo  |  CUSTOM=set your numbers";
        DrawText(footer,cx-MeasureText(footer,12)/2,SCREEN_H-24,12,(Color){75,75,100,255});

        /* ── Custom modal (drawn last, on top of everything) ── */
        if(custom_mode) draw_custom_modal(cx,cy,anim);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
