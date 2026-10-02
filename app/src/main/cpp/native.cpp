#include <jni.h>
#include <android/log.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BounceNative", __VA_ARGS__)

namespace {
constexpr int LOGICAL_W = 128;
constexpr int LOGICAL_H = 128;
constexpr int PLAYFIELD_H = 96;
constexpr int OFFSCREEN_W = 156;
constexpr int OFFSCREEN_H = 96;
constexpr int TILE = 12;
constexpr uint32_t BLUE = 0xFFB0E0F0u;
constexpr uint32_t DARK_BLUE = 0xFF1060B0u;

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> p;
};

struct LevelRecord { uint8_t px, py, ox, oy; int8_t dx, dy; uint8_t wx, wy; };
struct Level {
    uint8_t s=0,S=0,format=0,W=0,V=0,ao=0,width=0,height=0;
    std::vector<uint8_t> tiles;
    std::vector<LevelRecord> records;
    // The third level-header byte selects the player size, NOT the map grid.
    // The original b.class/f.class always address level cells on a 12px grid.
    static constexpr int tileSize() { return 12; }
    int playerSize() const { return format ? 16 : 12; }
    int widthPx() const { return width * 12; }
    int heightPx() const { return height * 12; }
};

struct ObjectState {
    LevelRecord base{};
    int x=0, y=0;
    int dx=0, dy=0;
};

class Reader {
    const uint8_t* p; size_t n, i=0;
public:
    Reader(const uint8_t* d, size_t sz): p(d), n(sz) {}
    uint8_t u8() { return i < n ? p[i++] : 0; }
};

Level parseLevel(const uint8_t* data, size_t size) {
    Reader r(data, size); Level l;
    l.s=r.u8(); l.S=r.u8(); l.format=r.u8(); l.W=r.u8(); l.V=r.u8(); l.ao=r.u8();
    l.width=r.u8(); l.height=r.u8();
    l.tiles.resize(static_cast<size_t>(l.width)*l.height);
    for (auto &v:l.tiles) v=r.u8();
    uint8_t count=r.u8(); l.records.resize(count);
    for (auto &o:l.records) {
        o.px=r.u8(); o.py=r.u8(); o.ox=r.u8(); o.oy=r.u8();
        o.dx=static_cast<int8_t>(r.u8()); o.dy=static_cast<int8_t>(r.u8());
        o.wx=r.u8(); o.wy=r.u8();
    }
    return l;
}

Image crop(const std::vector<uint32_t>& atlas, int aw, int ah, int x, int y) {
    Image out{12,12}; out.p.resize(144);
    for (int yy=0; yy<12; ++yy)
        for (int xx=0; xx<12; ++xx) {
            int sx=x*12+xx, sy=y*12+yy;
            out.p[yy*12+xx] = (sx>=0 && sy>=0 && sx<aw && sy<ah) ? atlas[sy*aw+sx] : 0;
        }
    return out;
}

Image transform(const Image& src, int t) {
    Image out{src.w,src.h}; out.p.resize(static_cast<size_t>(src.w)*src.h);
    for (int y=0;y<src.h;++y) for(int x=0;x<src.w;++x) {
        int dx=x,dy=y;
        switch(t) {
            case 0: dx=x; dy=y; break;
            case 1: dx=src.w-1-x; dy=y; break;       // mirror X
            case 2: dx=x; dy=src.h-1-y; break;       // mirror Y
            case 3: dx=src.h-1-y; dy=x; break;       // 90° CW
            case 4: dx=src.w-1-x; dy=src.h-1-y; break;
            case 5: dx=y; dy=src.w-1-x; break;       // 270° CW
            default: break;
        }
        if(dx>=0&&dy>=0&&dx<out.w&&dy<out.h) out.p[dy*out.w+dx]=src.p[y*src.w+x];
    }
    return out;
}

Image recolor(const Image& src, uint32_t color) {
    Image out=src;
    for(auto &px:out.p) if(((px>>24)&255)==0) px=color; else px=(px & 0x00FFFFFFu) | 0xFF000000u;
    return out;
}

Image compose16(const Image& src) {
    Image out{16,16}; out.p.assign(256,0);
    Image a=transform(src,0), b=transform(src,1), c=transform(src,2), d=transform(src,4);
    auto blit=[&](const Image& im,int ox,int oy){
        for(int y=0;y<im.h;++y) for(int x=0;x<im.w;++x){
            int dx=ox+x,dy=oy+y;
            if(dx>=0&&dy>=0&&dx<16&&dy<16) out.p[dy*16+dx]=im.p[y*im.w+x];
        }
    };
    blit(a,-4,-4); blit(b,8,-4); blit(c,-4,8); blit(d,8,8);
    return out;
}

// Exact Q[] construction from b.c().
std::array<Image,67> buildQ(const std::vector<uint32_t>& atlas, int aw, int ah) {
    std::array<Image,67> q;
    q[0]=crop(atlas,aw,ah,1,0);
    q[1]=crop(atlas,aw,ah,1,2);
    q[2]=recolor(crop(atlas,aw,ah,0,3),BLUE);
    q[3]=transform(q[2],1); q[4]=transform(q[2],3); q[5]=transform(q[2],5);
    q[6]=recolor(crop(atlas,aw,ah,0,3),DARK_BLUE);
    q[7]=transform(q[6],1); q[8]=transform(q[6],3); q[9]=transform(q[6],5);
    q[10]=crop(atlas,aw,ah,0,4); q[11]=crop(atlas,aw,ah,3,4);
    q[12]=crop(atlas,aw,ah,2,3); // special 24x48 image is not needed by tile map
    q[14]=crop(atlas,aw,ah,0,5); q[13]=transform(q[14],1); q[15]=transform(q[13],0); q[16]=transform(q[14],0);
    q[18]=crop(atlas,aw,ah,1,5); q[17]=transform(q[18],1); q[19]=transform(q[17],0); q[20]=transform(q[18],0);
    q[22]=crop(atlas,aw,ah,2,5); q[21]=transform(q[22],1); q[23]=transform(q[21],0); q[24]=transform(q[22],0);
    q[26]=crop(atlas,aw,ah,3,5); q[25]=transform(q[26],1); q[27]=transform(q[25],0); q[28]=transform(q[26],0);
    q[29]=transform(q[14],5); q[30]=transform(q[29],1); q[31]=transform(q[29],0); q[32]=transform(q[30],0);
    q[33]=transform(q[18],5); q[34]=transform(q[33],1); q[35]=transform(q[33],0); q[36]=transform(q[34],0);
    q[37]=transform(q[22],5); q[38]=transform(q[37],1); q[39]=transform(q[37],0); q[40]=transform(q[38],0);
    q[41]=transform(q[26],5); q[42]=transform(q[41],1); q[43]=transform(q[41],0); q[44]=transform(q[42],0);
    q[45]=crop(atlas,aw,ah,3,3); q[46]=crop(atlas,aw,ah,1,3); q[47]=crop(atlas,aw,ah,2,0); q[48]=crop(atlas,aw,ah,0,1);
    q[49]=compose16(crop(atlas,aw,ah,3,0));
    q[50]=crop(atlas,aw,ah,3,1); q[51]=crop(atlas,aw,ah,2,4); q[52]=crop(atlas,aw,ah,3,2);
    q[53]=crop(atlas,aw,ah,1,1); q[54]=crop(atlas,aw,ah,2,2);
    q[55]=recolor(crop(atlas,aw,ah,0,0),BLUE); q[56]=transform(q[55],3); q[57]=transform(q[55],4); q[58]=transform(q[55],5);
    q[59]=recolor(crop(atlas,aw,ah,0,0),DARK_BLUE); q[60]=transform(q[59],3); q[61]=transform(q[59],4); q[62]=transform(q[59],5);
    q[63]=crop(atlas,aw,ah,0,2); q[64]=transform(q[63],3); q[65]=transform(q[63],4); q[66]=transform(q[63],5);
    return q;
}

class Renderer {
    std::array<Image,67> q{};
    Image object24{};
    Level level{};
    std::vector<ObjectState> objects;
    std::vector<uint32_t> frame{LOGICAL_W*LOGICAL_H};
    int cameraX=0, cameraY=0;
    int playerWorldX=64, playerWorldY=48;
public:
    void setAssets(const jint* pixels, int w, int h) {
        std::vector<uint32_t> a(static_cast<size_t>(w)*h);
        std::memcpy(a.data(), pixels, a.size()*sizeof(uint32_t));
        q=buildQ(a,w,h);
        object24=Image{24,24}; object24.p.assign(24*24,0);
        auto blit24=[&](const Image& im,int ox,int oy,int t){
            Image tr=transform(im,t);
            for(int yy=0;yy<tr.h;++yy) for(int xx=0;xx<tr.w;++xx){
                int dx=ox+xx,dy=oy+yy;
                if(dx>=0&&dy>=0&&dx<24&&dy<24){
                    uint32_t px=tr.p[yy*tr.w+xx];
                    if((px>>24)!=0) object24.p[dy*24+dx]=px;
                }
            }
        };
        blit24(q[46],0,0,0); blit24(q[46],12,0,1);
        blit24(q[46],0,12,2); blit24(q[46],12,12,4);
    }
    void setLevel(const uint8_t* data,size_t n){
        level=parseLevel(data,n); cameraX=0; cameraY=0; objects.clear();
        objects.reserve(level.records.size());
        constexpr int ts=12;
        for(const auto& r: level.records){
            ObjectState o; o.base=r;
            o.x=static_cast<int>(r.px)*ts; o.y=static_cast<int>(r.py)*ts;
            o.dx=static_cast<int8_t>(r.dx); o.dy=static_cast<int8_t>(r.dy);
            objects.push_back(o);
        }
    }
    void setPlayerWorld(int x,int y){ playerWorldX=x; playerWorldY=y; }
    void updateObjects(){
        const int ts=level.tileSize();
        for(auto& o:objects){
            o.x += o.dx; o.y += o.dy;
            const int minX=static_cast<int>(o.base.px)*ts;
            const int minY=static_cast<int>(o.base.py)*ts;
            const int maxX=static_cast<int>(o.base.ox)*ts;
            const int maxY=static_cast<int>(o.base.oy)*ts;
            if(maxX>minX && (o.x<minX || o.x>maxX)){ o.x=std::clamp(o.x,minX,maxX); o.dx=-o.dx; }
            if(maxY>minY && (o.y<minY || o.y>maxY)){ o.y=std::clamp(o.y,minY,maxY); o.dy=-o.dy; }
        }
    }
    void clear(uint32_t c){std::fill(frame.begin(),frame.end(),c);}
    void blit(const Image& im,int dx,int dy) {
        for(int y=0;y<im.h;++y) for(int x=0;x<im.w;++x){
            int tx=dx+x,ty=dy+y; if(tx<0||ty<0||tx>=LOGICAL_W||ty>=PLAYFIELD_H) continue;
            uint32_t p=im.p[y*im.w+x]; if(((p>>24)&255)!=0) frame[ty*LOGICAL_W+tx]=p;
        }
    }
    void drawTile(int id,bool variant,int x,int y) {
        uint32_t bg=variant?DARK_BLUE:BLUE;
        switch(id) {
            case 0: for(int yy=0;yy<12;++yy)for(int xx=0;xx<12;++xx) if(x+xx>=0&&x+xx<LOGICAL_W&&y+yy>=0&&y+yy<PLAYFIELD_H) frame[(y+yy)*LOGICAL_W+x+xx]=bg; break;
            case 1: blit(q[0],x,y); break; case 2: blit(q[1],x,y); break;
            case 3: blit(q[variant?6:2],x,y); break;
            case 4: blit(q[variant?9:5],x,y); break;
            case 5: blit(q[variant?7:3],x,y); break;
            case 6: blit(q[variant?8:4],x,y); break;
            case 7: blit(q[10],x,y); break; case 8: blit(q[11],x,y); break;
            case 9: case 10: break;
            case 11: case 12: break;
            case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: {
                static const int A[16]={35,36,17,19,43,44,25,27,31,32,13,15,39,40,21,23};
                static const int B[16]={33,34,18,20,41,42,26,28,29,30,14,16,37,38,22,24};
                int i=id-13; for(int yy=0;yy<12;++yy)for(int xx=0;xx<12;++xx)if(x+xx>=0&&x+xx<LOGICAL_W&&y+yy>=0&&y+yy<PLAYFIELD_H)frame[(y+yy)*LOGICAL_W+x+xx]=bg;
                blit(q[A[i]],x,y); blit(q[B[i]],x,y); break;
            }
            case 29: blit(q[45],x,y); break;
            case 30: blit(variant?q[61]:q[57],x,y); break;
            case 31: blit(variant?q[60]:q[56],x,y); break;
            case 32: blit(variant?q[59]:q[55],x,y); break;
            case 33: blit(variant?q[62]:q[58],x,y); break;
            case 34: blit(q[65],x,y); break;
            case 35: blit(q[64],x,y); break;
            case 36: blit(q[63],x,y); break;
            case 37: blit(q[66],x,y); break;
            case 38: blit(q[53],x,y); break;
            case 39: blit(q[50],x,y); break;
            case 40: blit(q[50],x,y); break;
            case 41: blit(transform(q[50],5),x,y); break;
            case 42: blit(transform(q[50],4),x,y); break;
            case 43: blit(transform(q[50],3),x,y); break;
            case 44: blit(q[51],x,y); break;
            case 45: blit(transform(q[51],5),x,y); break;
            case 46: blit(transform(q[51],4),x,y); break;
            case 47: blit(transform(q[51],3),x,y); break;
            case 48: blit(q[52],x,y); break;
            case 49: blit(transform(q[52],5),x,y); break;
            case 50: blit(transform(q[52],4),x,y); break;
            case 51: blit(transform(q[52],3),x,y); break;
            case 52: blit(q[54],x,y); break;
            case 53: blit(transform(q[54],5),x,y); break;
            case 54: blit(transform(q[54],4),x,y); break;
            default: break;
        }
    }
    void render() {
        clear(BLUE);
        if(level.width==0||level.height==0) return;
        constexpr int ts=12;
        const int worldW=level.width*ts;
        const int worldH=level.height*ts;
        const int viewW=OFFSCREEN_W;
        const int viewH=OFFSCREEN_H;
        // Exact camera behavior from e.e(): horizontal camera is quantized to
        // the 12px map grid; vertical camera is quantized in 7px increments.
        int rawX=playerWorldX-64;
        rawX=std::clamp(rawX,0,std::max(0,worldW-viewW));
        cameraX=(rawX/12)*12;

        // Exact vertical camera loop from e.e(): start at k=0 and move the
        // camera in 7px increments until the player's 96px window contains it.
        int camY=0;
        while (playerWorldY-6 < camY) camY-=7;
        while (playerWorldY+6 > camY+96) camY+=7;
        cameraY=camY;

        const int startX=cameraX/ts;
        const int startY=cameraY/ts;
        const int cols=OFFSCREEN_W/ts+1;
        const int rows=OFFSCREEN_H/ts+1;
        for(int ty=0;ty<rows && startY+ty<level.height;++ty)
            for(int tx=0;tx<cols && startX+tx<level.width;++tx) {
                const uint8_t raw=level.tiles[(startY+ty)*level.width+(startX+tx)];
                const bool variant=(raw&0x40)!=0;
                const int id=raw&0x3F;
                const int sx=tx*ts-(cameraX%ts);
                const int sy=ty*ts-(cameraY%ts);
                // The level map is always a 12px grid. The header's format byte
                // only changes the player collision/sprite size.
                drawTile(id,variant,sx,sy);
            }
        updateObjects();
        for(const auto& o:objects){
            const int ox=o.x-cameraX, oy=o.y-cameraY;
            if(ox>=-24 && ox<LOGICAL_W && oy>=-24 && oy<PLAYFIELD_H) blit(object24,ox,oy);
        }
        // HUD: reserve the original 32px strip. Score/lives are supplied by the
        // game controller in the next pass; keep the strip opaque rather than fake text.
        for(int y=PLAYFIELD_H;y<LOGICAL_H;++y)
            for(int x=0;x<LOGICAL_W;++x) frame[y*LOGICAL_W+x]=0xFF000000u;
    }
    void drawPlayer(int x,int y,int frameIndex=47) { if(frameIndex>=0 && frameIndex<67) blit(q[frameIndex],x-cameraX,y-cameraY); }
    const Level& currentLevel() const { return level; }
    const std::vector<uint32_t>& pixels(){return frame;}
};

struct Player {
    // Direct translation of f.class' persistent movement fields.
    int x=64, y=48;          // s, r
    int vx=0, vy=0;           // l, o
    int size=12, half=6;      // a, p
    int input=0;              // w: 1=left, 2=right, 4=up, 8=action/down
    int t=0, h=0, g=0, yState=0, anim=0;
    bool m=false, v=false, u=false;
    int state=1;              // z: 1 normal, 2 death animation
    bool invincible=false;

    static constexpr uint8_t mask12[12][12] = {
        {0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,1,1,1,1,1,1,1,1,0,0},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0}
    };
    static constexpr uint8_t mask16[16][12] = {
        {0,0,0,0,0,1,1,1,1,1,1,0},
        {0,0,0,1,1,1,1,1,1,1,1,1},
        {0,0,1,1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,1,1,1,1,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,0},
        {0,0,0,0,0,1,1,1,1,1,1,0}
    };

    static bool solid(uint8_t raw) {
        const int t=raw & 0x3f;
        // Exact non-collision base cases in f.a(int,int,int,int).
        return !(t==0 || t==8 || t==9 || t==10 || t==11 || t==12);
    }

    // Direct translation of f.b(int,int,int,int): tests the original player
    // collision mask against one 12x12 map cell. The J2ME engine always uses
    // 12-pixel map cells here, including the 16-pixel player variant.
    bool collidesCell(const Level& lv, int px, int py, int row, int col) const {
        if (row < 0 || row >= lv.height || col < 0 || col >= lv.width)
            return false;
        if (state == 2)
            return false;

        const int cellX = col * 12;
        const int cellY = row * 12;
        const int ox = px - half - cellX;
        const int oy = py - half - cellY;

        int sx, sy, ex, ey;
        if (ox >= 0) { sx = ox; ex = 12; }
        else { sx = 0; ex = size + ox; }
        if (oy >= 0) { sy = oy; ey = 12; }
        else { sy = 0; ey = size + oy; }

        sx = std::max(0, sx); sy = std::max(0, sy);
        ex = std::min(12, ex); ey = std::min(12, ey);
        if (sx >= ex || sy >= ey)
            return false;

        const auto& mask = (size == 16) ? mask16 : mask12;
        for (int yy = sy; yy < ey; ++yy) {
            for (int xx = sx; xx < ex; ++xx) {
                const int mx = xx - ox;
                const int my = yy - oy;
                if (my >= 0 && my < (size == 16 ? 16 : 12) &&
                    mx >= 0 && mx < 12 && mask[my][mx])
                    return true;
            }
        }
        return false;
    }

    // The original f.b(...) first tests the player's mask against a single
    // map cell. This wrapper checks the neighboring cells touched by the
    // player's bounding box, retaining the original 12-pixel grid.
    bool collidesAt(const Level& lv, int px, int py) const {
        const int left   = px - half;
        const int top    = py - half;
        const int right  = px + half - 1;
        const int bottom = py + half - 1;
        const int c0 = std::max(0, left / 12);
        const int r0 = std::max(0, top / 12);
        const int c1 = std::min<int>(lv.width - 1, right / 12);
        const int r1 = std::min<int>(lv.height - 1, bottom / 12);

        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const uint8_t raw = lv.tiles[row * lv.width + col];
                const int tile = raw & 0x3F;
                // The original generic mask test is only reached for terrain
                // types which participate in collision. Empty/background and
                // the dynamic/special non-solid types are handled separately.
                if (tile == 0 || tile == 8 || tile == 9 ||
                    tile == 10 || tile == 11 || tile == 12)
                    continue;
                if (collidesCell(lv, px, py, row, col))
                    return true;
            }
        }
        return false;
    }

    void update(const Level& lv) {
        const int ts=12;
        size=lv.playerSize(); half=size/2;
        if(state==2) return;

        // Match f.b(): choose vertical acceleration from the tile underneath.
        const int cx=x/12, cy=y/12;
        bool flipped=false;
        if(cy>=0 && cy<lv.height && cx>=0 && cx<lv.width)
            flipped=(lv.tiles[cy*lv.width+cx] & 0x40)!=0;

        int accelY, accelX;
        if(flipped) {
            if(size==16) { accelY=-30; accelX=-2; }
            else { accelY=42; accelX=6; }
            if(m) vy=-10;
        } else if(size==16) { accelY=38; accelX=3; }
        else { accelY=80; accelX=4; }

        bool specialMotion=false;
        if(g>0) {
            specialMotion=true;
            accelY=-accelY; accelX=-accelX;
            --g;
            if(g==0) m=false;
        }

        if(yState>0) {
            int oldAbs=std::abs(t);
            if(-oldAbs <= -80) t=specialMotion ? 80 : -80;
            --yState;
        }

        ++anim; if(anim==3) anim=0;
        vy=std::clamp(vy,-150,150);
        vx=std::clamp(vx,-150,150);
        if(vy>0 && vy<10 && !flipped && !specialMotion) vy=10;

        // Exact vertical sub-stepping structure from f.b().
        const int vsteps=std::abs(vy)/10;
        for(int step=0; step<vsteps; ++step) {
            int dir=(vy==0)?0:(vy<0?-1:1);
            if(collidesAt(lv,x,y+dir)) {
                y+=dir;
                m=false;
                if(accelY==-30) {
                    const int ty=y/12;
                    if(ty>=0 && ty<lv.height && cx>=0 && cx<lv.width &&
                       !(lv.tiles[ty*lv.width+cx]&0x40)) {
                        vy >>= 1;
                        if(vy<=10 && vy>=-10) vy=0;
                    }
                }
            } else {
                if(u && vx<10 && anim==0) {
                    if(collidesAt(lv,x+1,y+dir)) { x+=1; y+=dir; u=false; }
                    else if(collidesAt(lv,x-1,y+dir)) { x-=1; y+=dir; u=false; }
                }
                if(dir<=0 || specialMotion) {
                    vy=-vy/2;
                    m=true;
                    if(v && (input&8)) { v=false; t += specialMotion ? 10 : -10; }
                    else if(yState==0) t=0;
                    if(vy<10 && vy>-10) vy=specialMotion ? -10 : 10;
                } else if(specialMotion) {
                    vy=-20;
                } else {
                    vy=-vy/2;
                }
            }
        }

        // Special movement settling from the original b().
        if(specialMotion) {
            if(accelX==-2 && vy<accelY) { vy += accelX; if(vy>accelY) vy=accelY; }
            else if(!m && vy>accelY) { vy += accelX; if(vy<accelY) vy=accelY; }
        } else if(!m && vy>accelY) {
            vy += accelX; if(vy<accelY) vy=accelY;
        }

        // Horizontal acceleration/deceleration from the original final block.
        const int limit = yState ? 100 : 50;
        if(input&2) {
            if(vx<limit) vx+=6;
        } else if(input&1) {
            if(vx>-limit) vx-=6;
        } else if(vx>0) vx=std::max(0,vx-4);
        else if(vx<0) vx=std::min(0,vx+4);

        // Exact horizontal sub-step count: abs(vx)/10.
        const int hsteps=std::abs(vx)/10;
        for(int step=0; step<hsteps; ++step) {
            const int dir=(vx>0)-(vx<0);
            if(!dir) break;
            if(collidesAt(lv,x+dir,y)) {
                if(u) {
                    u=false;
                    if(collidesAt(lv,x+dir,y+1)) { x+=dir; y+=1; }
                    else if(collidesAt(lv,x+dir,y-1)) { x+=dir; y-=1; }
                    else vx=-vx/2;
                } else vx=-vx/2;
            } else x+=dir;
        }
    }
};

class Cheats {
    int state=0; bool advanced=false; int pendingLevelDelta=0; bool pendingComplete=false;
public:
    bool invincible=false; int flyG=0;
    int key(int k) {
        switch(k){
        case '7': state=(state==0||state==2)?state+1:0; break;
        case '8': if(state==1||state==3)++state; else if(state==5){invincible=true;state=0;} else state=0; break;
        case '9': if(state==4){advanced=true;state=0;}else state=0;break;
        case '1': if(advanced) pendingLevelDelta=-1; break;
        case '3': if(advanced) pendingLevelDelta=1; break;
        case '5': if(advanced) invincible=true; break;
        case '#': if(advanced) flyG=300; break;
        default: state=0;break;
        }
        return 0;
    }
    int consumeLevelDelta(){int d=pendingLevelDelta;pendingLevelDelta=0;return d;}
    int consumeFlyG(){int v=flyG; flyG=0; return v;}
    bool consumeComplete(){bool v=pendingComplete;pendingComplete=false;return v;}
    bool advancedMode()const{return advanced;}
};

Renderer renderer; Player player; Cheats cheats;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pavan3999_bounce_MainActivity_nativeInit(JNIEnv* env,jobject,jintArray atlas,jbyteArray levelBytes){
    jsize n=env->GetArrayLength(atlas); if(n<48*72) return;
    jint* ap=env->GetIntArrayElements(atlas,nullptr); renderer.setAssets(ap,48,72); env->ReleaseIntArrayElements(atlas,ap,JNI_ABORT);
    jsize ln=env->GetArrayLength(levelBytes); jbyte* lp=env->GetByteArrayElements(levelBytes,nullptr);
    renderer.setLevel(reinterpret_cast<const uint8_t*>(lp),ln); env->ReleaseByteArrayElements(levelBytes,lp,JNI_ABORT);
    const auto& lv=renderer.currentLevel();
    LOGI("Native renderer initialized: level %dx%d tile=%d",lv.width,lv.height,lv.tileSize());
}

extern "C" JNIEXPORT void JNICALL
Java_com_pavan3999_bounce_MainActivity_nativeLoadLevel(JNIEnv* env,jobject,jbyteArray levelBytes){
    jsize ln=env->GetArrayLength(levelBytes);
    jbyte* lp=env->GetByteArrayElements(levelBytes,nullptr);
    renderer.setLevel(reinterpret_cast<const uint8_t*>(lp),ln);
    env->ReleaseByteArrayElements(levelBytes,lp,JNI_ABORT);
    player.x=64; player.y=48; player.vx=player.vy=0; player.input=0;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_pavan3999_bounce_MainActivity_nativeFrame(JNIEnv* env,jobject){
    player.invincible = cheats.invincible;
    const int cheatG = cheats.consumeFlyG();
    if (cheatG) player.g = cheatG;
    player.update(renderer.currentLevel());
    renderer.setPlayerWorld(player.x,player.y);
    renderer.render();
    renderer.drawPlayer(player.x, player.y, 47);
    const auto& p=renderer.pixels(); jintArray out=env->NewIntArray(static_cast<jsize>(p.size())); env->SetIntArrayRegion(out,0,p.size(),reinterpret_cast<const jint*>(p.data())); return out;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_pavan3999_bounce_MainActivity_nativeKey(JNIEnv*,jobject,jint key){
    int k=0;
    switch(key){
        case 7:k='7';break; case 8:k='8';break; case 9:k='9';break;
        case 1:k='1';break; case 3:k='3';break; case 5:k='5';break;
        case 12:k='#';break; default: break;
    }
    if(k) cheats.key(k);
    if(key==21) player.input|=1;       // LEFT
    if(key==22) player.input|=2;       // RIGHT
    if(key==23) player.input|=4;       // UP / action hook
    if(key==24) player.input|=8;       // DOWN / action hook
    if(key==-21) player.input&=~1;
    if(key==-22) player.input&=~2;
    return cheats.consumeLevelDelta();
}

extern "C" JNIEXPORT void JNICALL
Java_com_pavan3999_bounce_MainActivity_nativeTouch(JNIEnv*,jobject,jfloat x,jfloat y,jint action){
    if(action==1||action==3) { player.input=0; return; }
    if(y>PLAYFIELD_H) {
        if(x<42) player.input|=1; else if(x>86) player.input|=2;
    }
}
