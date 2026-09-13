#include "render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RGB(r,g,b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define BG RGB(12,20,30)
#define FG RGB(225,236,242)
#define MUTED RGB(132,159,176)
#define GREEN RGB(97,218,161)

/* Five columns per 5x7 glyph; bit zero is the uppermost row. */
static const unsigned char letters[26][5] = {
 {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {7,8,112,8,7},{97,81,73,69,67}
};
static const unsigned char lowercase[26][5] = {
 {32,84,84,84,120},{127,72,68,68,56},{56,68,68,68,32},{56,68,68,72,127},
 {56,84,84,84,24},{8,126,9,1,2},{12,82,82,82,62},{127,8,4,4,120},
 {0,68,125,64,0},{32,64,68,61,0},{127,16,40,68,0},{0,65,127,64,0},
 {124,4,24,4,120},{124,8,4,4,120},{56,68,68,68,56},{124,20,20,20,8},
 {8,20,20,24,124},{124,8,4,4,8},{72,84,84,84,32},{4,63,68,64,32},
 {60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},
 {12,80,80,80,60},{68,100,84,76,68}
};
static const unsigned char digits[10][5] = {
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},
 {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
 {54,73,73,73,54},{6,73,73,41,30}
};
static void pixel(NFRender *r, int x, int y, uint16_t color)
{
    if ((unsigned)x < 320 && y >= r->clip_top && y < r->clip_bottom)
        r->pixels[y * 320 + x] = color;
}
static void rect(NFRender *r, int x, int y, int width, int height, uint16_t color)
{
    int xx, yy, right = x + width, bottom = y + height;
    if (x < 0) x = 0;
    if (y < r->clip_top) y = r->clip_top;
    if (right > 320) right = 320;
    if (bottom > r->clip_bottom) bottom = r->clip_bottom;
    for (yy = y; yy < bottom; ++yy)
        for (xx = x; xx < right; ++xx) r->pixels[yy * 320 + xx] = color;
}
static void border(NFRender *r, int x, int y, int width, int height, uint16_t color)
{
    rect(r,x,y,width,1,color); rect(r,x,y+height-1,width,1,color);
    rect(r,x,y,1,height,color); rect(r,x+width-1,y,1,height,color);
}
static void line(NFRender *r, int x, int y, int x2, int y2, uint16_t color)
{
    int dx = abs(x2-x), sx = x < x2 ? 1 : -1;
    int dy = -abs(y2-y), sy = y < y2 ? 1 : -1, error = dx+dy;
    for (;;) {
        int e;
        pixel(r,x,y,color);
        if (x == x2 && y == y2) break;
        e = 2*error;
        if (e >= dy) { error += dy; x += sx; }
        if (e <= dx) { error += dx; y += sy; }
    }
}
static void text(NFRender *r, int x, int y, const char *s, uint16_t color)
{
    for (; *s && x < 320; ++s, x += 6) {
        unsigned char custom[5] = {0};
        const unsigned char *glyph = custom;
        int c = (unsigned char)*s, col, row;
        if (c >= 'A' && c <= 'Z') glyph = letters[c-'A'];
        else if (c >= 'a' && c <= 'z') glyph = lowercase[c-'a'];
        else if (c >= '0' && c <= '9') glyph = digits[c-'0'];
        else switch(c) {
        case ':': custom[1]=custom[2]=36; break;
        case '.': custom[2]=64; break;
        case ',': custom[2]=96; break;
        case '-': custom[0]=custom[1]=custom[2]=custom[3]=custom[4]=8; break;
        case '_': custom[0]=custom[1]=custom[2]=custom[3]=custom[4]=64; break;
        case '~': custom[0]=8;custom[1]=4;custom[2]=8;custom[3]=16;custom[4]=8;break;
        case '/': custom[0]=32;custom[1]=16;custom[2]=8;custom[3]=4;custom[4]=2;break;
        case '+': custom[0]=custom[1]=custom[3]=custom[4]=8;custom[2]=62;break;
        case '=': custom[0]=custom[1]=custom[2]=custom[3]=custom[4]=20;break;
        case '%': custom[0]=99;custom[1]=19;custom[2]=8;custom[3]=100;custom[4]=99;break;
        case '(': custom[2]=62;custom[3]=65;break;
        case ')': custom[1]=65;custom[2]=62;break;
        case '>': custom[1]=65;custom[2]=34;custom[3]=28;break;
        case '<': custom[1]=28;custom[2]=34;custom[3]=65;break;
        case '?': custom[0]=2;custom[1]=1;custom[2]=81;custom[3]=9;custom[4]=6;break;
        default: break;
        }
        for (col=0;col<5;++col) for(row=0;row<7;++row)
            if (glyph[col] & (1<<row)) pixel(r,x+col,y+row,color);
    }
}
static void wrapped(NFRender *r, int y, const char *message, uint16_t color)
{
    while (*message && y < 221) {
        char buffer[50];
        size_t n = strlen(message), split;
        if(n > 49) n = 49;
        split = n;
        if(message[n]) while(split && message[split] != ' ') --split;
        if(!split) split = n;
        memcpy(buffer,message,split);buffer[split]=0;
        text(r,12,y,buffer,color);
        message += split;
        while(*message==' ')++message;
        y += 12;
    }
}
static void ellipse(NFRender *r, float cx, float cy, float rx, float ry, float c, float s, uint16_t color)
{
    int radius = (int)(rx + ry + 1), x, y;
    float ix = 1/(rx*rx), iy = 1/(ry*ry);
    for(y=-radius;y<=radius;++y) for(x=-radius;x<=radius;++x) {
        float u=x*c-y*s,v=x*s+y*c;
        if(u*u*ix+v*v*iy<=1) {
            int py=(int)cy+y;
            if(py>=24&&py<184)pixel(r,(int)cx+x,py,color);
        }
    }
}
static void local_line(NFRender *r, const NFWorld *w, float a, float b, float d, float e, uint16_t color)
{
    float c=cosf(w->direction),s=sinf(w->direction),x=w->x/3,y=24+w->y/3;
    int y1=(int)(y-a*s+b*c),y2=(int)(y-d*s+e*c);
    if(y1<24)y1=24;
    if(y1>183)y1=183;
    if(y2<24)y2=24;
    if(y2>183)y2=183;
    line(r,(int)(x+a*c+b*s),y1,(int)(x+d*c+e*s),y2,color);
}
static void body_ellipse(NFRender *r, const NFWorld *w, float a,float b,float rx,float ry,uint16_t color)
{
    float c=cosf(w->direction),s=sinf(w->direction);
    ellipse(r,w->x/3+a*c+b*s,24+w->y/3-a*s+b*c,rx,ry,c,s,color);
}
static void draw_fly(NFRender *r,const NFWorld *w)
{
    int side,pair;
    float phase=r->walk_phase;
    for(side=-1;side<=1;side+=2) for(pair=0;pair<3;++pair) {
        float a=4-pair*4, sway=sinf(phase+pair*2.1f+side*1.5f)*2;
        float tip=a-2+sway;
        if(w->behavior==NF_GROOM&&pair==0)tip=9+sinf(r->groom_phase)*3;
        local_line(r,w,a,side*2,a-2,side*7,RGB(133,154,160));
        local_line(r,w,a-2,side*7,tip,side*12,RGB(167,185,186));
    }
    body_ellipse(r,w,-5,0,8,4,RGB(127,91,49));
    local_line(r,w,-9,-2,-9,2,RGB(60,49,37));
    local_line(r,w,-6,-3,-6,3,RGB(60,49,37));
    local_line(r,w,-3,-3,-3,3,RGB(60,49,37));
    for(side=-1;side<=1;side+=2) {
        float spread=r->wing_spread;
        if(w->behavior==NF_FLY)spread+=sinf((float)w->time_ms*.13f)*2;
        body_ellipse(r,w,-4,side*spread,8,2.5f,RGB(159,183,184));
        local_line(r,w,2,side*2,-10,side*spread,RGB(208,220,211));
    }
    body_ellipse(r,w,1,0,4.7f,3.7f,RGB(102,81,55));
    body_ellipse(r,w,7,0,3.5f,3,RGB(87,73,52));
    body_ellipse(r,w,8,-2.5f,2,1.5f,RGB(221,91,62));
    body_ellipse(r,w,8,2.5f,2,1.5f,RGB(221,91,62));
    local_line(r,w,10,-1,13,-3,RGB(186,173,122));
    local_line(r,w,10,1,13,3,RGB(186,173,122));
    if(r->proboscis>.1f)local_line(r,w,10,0,10+r->proboscis*6,0,RGB(237,185,111));
}
static uint16_t region_color(unsigned region)
{
    static const uint16_t colors[4]={RGB(92,188,248),RGB(188,147,243),RGB(250,189,98),RGB(102,220,163)};
    return colors[region<4?region:0];
}
static void neural_view(NFRender *r,const NFBrain *b,const NFWorld *w,const NFUI *ui)
{
    unsigned g;
    char buffer[64];
    text(r,5,27,"Sensory  Central  Drives  Motor",MUTED);
    for(g=0;g<NF_GROUP_COUNT;++g) {
        int x=3+(g%NF_NEURAL_COLUMNS)*35,y=40+(g/NF_NEURAL_COLUMNS)*15;
        uint32_t size=b->offset[g+1]-b->offset[g];
        uint16_t color=region_color(nf_group_regions[g]);
        rect(r,x,y,32,12,RGB(30,43,57));
        if(size) {
            unsigned width=b->spikes[g]*30/size;
            if(width>30)width=30;
            rect(r,x+1,y+9,(int)width,2,color);
        }
        snprintf(buffer,sizeof(buffer),"%02u",g);
        text(r,x+2,y+1,buffer,size?color:MUTED);
        if(b->active[g])pixel(r,x+28,y+3,GREEN);
        if(g==ui->selected_group)border(r,x-1,y-1,34,14,FG);
    }
    g=ui->selected_group;
    text(r,5,148,nf_group_names[g],region_color(nf_group_regions[g]));
    snprintf(buffer,sizeof(buffer),"N:%lu Spikes:%lu",(unsigned long)(b->offset[g+1]-b->offset[g]),(unsigned long)b->spikes[g]);
    text(r,5,160,buffer,FG);
    snprintf(buffer,sizeof(buffer),"Act:%u%%  Arrows: select  B: arena",(unsigned)fminf(999,w->activity[g]));
    text(r,5,172,buffer,MUTED);
}
static void help(NFRender *r)
{
    static const char *const lines[]={
        "nFly / Full FlyWire FAFB v783",
        "Arrows       Move food cursor",
        "F, Enter     Food at the fly",
        "Enter        Place food at cursor",
        "1/2/3/4      Head/thorax/abdomen/legs",
        "A / Shift+A  Gentle / strong wind",
        "             From cursor toward fly",
        "L            Bright / dim / dark",
        "T            Neutral / warm / cool",
        "D            Danger odor on/off",
        "C            Clear food",
        "P            Pause / resume",
        "B            Neural group view",
        "Arrows       Select group in brain",
        "R            Reset simulation",
        "H            Close help",
        "Esc          Exit to documents",
        "139255 LIF neurons / 2698236 edges",
        "Brain data + upstream virtual VNC",
        "Clock shows simulation time."
    };
    unsigned i;
    rect(r,0,0,320,240,BG);
    for(i=0;i<sizeof(lines)/sizeof(lines[0]);++i)text(r,7,9+(int)i*11,lines[i],i==0?GREEN:FG);
}
void nf_render_init(NFRender *r,uint16_t *pixels)
{
    memset(r,0,sizeof(*r));r->pixels=pixels;r->wing_spread=3;
    r->clip_bottom = 240;
}
void nf_render_loading(NFRender *r,const char *stage,uint32_t done,uint32_t total)
{
    char buffer[40];
    rect(r,0,0,320,240,BG);
    text(r,16,28,"nFly / Full connectome",GREEN);
    text(r,16,52,"139255 neurons / 2698236 edges",FG);
    text(r,16,89,stage,FG);
    border(r,16,109,288,14,MUTED);
    if(total)rect(r,18,111,(int)((uint64_t)done*284/total),10,GREEN);
    snprintf(buffer,sizeof(buffer),"%lu / %lu",(unsigned long)done,(unsigned long)total);
    text(r,16,136,buffer,MUTED);
    text(r,16,176,"Loading all connections into RAM",MUTED);
}
void nf_render_error(NFRender *r,const char *message)
{
    rect(r,0,0,320,240,BG);
    text(r,12,20,"nFly / Load error",RGB(243,138,117));
    wrapped(r,50,message,FG);
    text(r,12,228,"Enter / Esc: return to OS",MUTED);
}
void nf_render_frame(NFRender *r,const NFBrain *b,const NFWorld *w,const NFUI *ui)
{
    char buffer[96];
    unsigned i;
    uint32_t elapsed=w->time_ms-r->last_time;
    const char *light=w->light==1?"bright":w->light>0?"dim":"dark";
    const char *temp=w->temperature==.5f?"neutral":w->temperature>.5f?"warm":"cool";
    float drives[5]={w->drives.hunger,w->drives.fear,w->drives.fatigue,w->drives.curiosity,w->drives.groom};
    static const char *const drive_names[5]={"Hunger","Fear","Tired","Seek","Groom"};
    r->last_time=w->time_ms;
    r->walk_phase+=(float)elapsed*.025f*fminf(3,w->speed);
    r->groom_phase+=(float)elapsed*.018f;
    if(elapsed) {
        r->wing_spread=w->behavior==NF_FLY?8:3;
        r->proboscis=w->behavior==NF_FEED?1:0;
    }
    if(ui->help){help(r);return;}
    rect(r,0,0,320,240,BG);
    text(r,5,3,"nFly",GREEN);
    snprintf(buffer,sizeof(buffer),"Full %luN / %luE%s",(unsigned long)b->neurons,(unsigned long)b->edges,ui->paused?" Pause":"");
    text(r,41,3,buffer,FG);
    snprintf(buffer,sizeof(buffer),"L:%s T:%s D:%s  %lu.%lus",light,temp,w->danger?"on":"off",(unsigned long)(w->time_ms/1000),(unsigned long)(w->time_ms/100%10));
    text(r,5,14,buffer,MUTED);
    if(ui->neural_view)neural_view(r,b,w,ui);
    else {
        uint16_t ground=w->light==0?RGB(16,24,32):w->light<1?RGB(25,37,43):RGB(36,51,53);
        r->clip_top = 24; r->clip_bottom = 184;
        rect(r,0,24,320,160,ground);
        for(i=16;i<320;i+=32) { unsigned y;for(y=32;y<184;y+=24)pixel(r,(int)i,(int)y,RGB(61,75,74)); }
        for(i=0;i<w->food_count;++i) {
            const NFFood *f=&w->food[i];
            float radius=3.5f*(1-f->eaten*.9f);
            ellipse(r,f->x/3,24+f->y/3,radius,radius,1,0,GREEN);
        }
        draw_fly(r,w);
        {
            int x=(int)(ui->cursor_x/3),y=24+(int)(ui->cursor_y/3);
            line(r,x-5,y,x-2,y,RGB(237,212,121));line(r,x+2,y,x+5,y,RGB(237,212,121));
            line(r,x,y-5,x,y-2,RGB(237,212,121));line(r,x,y+2,x,y+5,RGB(237,212,121));
        }
        if((int32_t)(w->touch_until-w->time_ms)>0) {
            int x=(int)(w->x/3),y=24+(int)(w->y/3);
            border(r,x-17,y-17,35,35,RGB(229,138,103));
        }
        if((int32_t)(w->wind_until-w->time_ms)>0) {
            int x=(int)(w->x/3),y=24+(int)(w->y/3);
            int dx=(int)(cosf(w->wind_direction)*22),dy=(int)(-sinf(w->wind_direction)*22);
            line(r,x-dx,y-dy,x,y,RGB(124,202,232));
        }
        text(r,5,28,nf_behavior_names[w->behavior],FG);
        text(r,5,174,ui->action?ui->action:"",MUTED);
        r->clip_top = 0; r->clip_bottom = 240;
    }
    snprintf(buffer,sizeof(buffer),"Spikes:%lu  Tick:%lu  %s%lums",(unsigned long)b->fired_count,(unsigned long)b->ticks,ui->tick_measured?"":"~",(unsigned long)ui->tick_ms);
    text(r,5,188,buffer,FG);
    for(i=0;i<5;++i) {
        int x=5+(int)i*63;
        text(r,x,201,drive_names[i],MUTED);
        rect(r,x,212,57,5,RGB(41,54,65));
        rect(r,x,212,(int)(drives[i]*57),5,i==1?RGB(231,144,102):GREEN);
    }
    text(r,5,228,"Enter:food  P:pause  B:brain  H:help  Esc:exit",MUTED);
}
